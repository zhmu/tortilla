#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include "exceptions.h"
#include "file.h"
#include "macros.h"

using namespace std;
using namespace boost::interprocess;

File::File(std::string path, off_t len, std::string root_path)
{
  rootpath = root_path; filename = path; length = len; reopened = false; lastInteraction = time(NULL);

	/*
	 * First of all, try to open the file; if this works, we know the
	 * file pre-existed and we should refetch all of it (hopefully)
	 */
	string fullpath = rootpath + filename;
	makePath(fullpath);
	if ((fd = ::open(fullpath.c_str(), O_RDWR)) < 0) {
		/* This failed; attempt to create the file */
		if ((fd = ::open(fullpath.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644)) < 0)
			throw FileException("unable to create '" + fullpath + "'");
	} else {
		/* We reopened the file, but maybe the length is invalid */
		off_t filesize = lseek(fd, 0, SEEK_END);
		if (filesize == len) {
			/* All done - just don't forget to close the file as outlined below */
			reopened = true;
			close();
			return;
		}

		/*
		 * File size mismatch - throw the file away and recreate it. We
		 * us an offset of 0 as this seems the safest bet; we enlarge
		 * the file to what we want below.
		 */
		ftruncate(fd, 0);
	}

	/*
	 * Expand the file to the requested length by seeking almost to the
	 * end and writing a byte there.
	 */
	uint8_t b = 0xFF;
	lseek(fd, len - 1, SEEK_SET);
	if (!::write(fd, &b, 1))
		throw FileException("unable to expand file");

	/*
	 * Close the file - it will be reopened as needed, and this ensures we could
	 * access the file once the object was created.
	 */
	close();
}
	
void
File::write(off_t offset, const void* buf, size_t len)
{
	assert(offset + (off_t)len <= length);
	assert(isOpened());

	/*
	 * Don't consider short writes a failure if the call was interrupted;
	 * this happens if the user hits ^C to exit.
	 */
	lastInteraction = time(NULL);
	if ((size_t)pwrite(fd, buf, len, offset) != len && errno != EINTR)
		throw FileException("short write");
}

void
File::read(off_t offset, void* buf, size_t len)
{
	assert(offset + (off_t)len <= length);
	assert(isOpened());

	/*
	 * Don't consider short writes a failure if the call was interrupted;
	 * this happens if the user hits ^C to exit.
	 */
	lastInteraction = time(NULL);
	if ((size_t)pread(fd, buf, len, offset) != len && errno != EINTR)
		throw FileException("short read");
}

File::~File()
{
	/* Lock the file before closing it; this ensures we wait until consumers are done */
	scoped_lock<interprocess_upgradable_mutex> lock(rwl_file);
	close();
}

bool
File::isOpened()
{
	return (fd >= 0);
}

void
File::open()
{
	if (fd >= 0)
		return;

	string fullpath = rootpath + filename;
	if ((fd = ::open(fullpath.c_str(), O_RDWR)) < 0) {
		/*
		 * If we couldn't re-open the file, that's weird. We could do it in the
	 	 * constructor...
		 */
		throw FileException("unable to re-open '" + fullpath + "'");
	}
}

void
File::close()
{
	if (fd < 0)
		return;

	::close(fd);
	fd = -1;
}

void
File::lockRead()
{
	RLOCK(file);
	read_locked = true;
}

void
File::lockWrite()
{
	WLOCK(file);
	read_locked = false;
}

void
File::unlock()
{
	if (read_locked)
		RUNLOCK(file);
	else
		WUNLOCK(file);
}

bool
File::compareByLastInteraction(File* a, File* b)
{
	return a->getLastInteraction() < b->getLastInteraction();
}

bool
File::rename(std::string newpath)
{
	WLOCK(file);
	if (::rename(string(rootpath + filename).c_str(), string(rootpath + newpath).c_str()) < 0) {
		WUNLOCK(file);
		return false;
	}
	filename = newpath;
	WUNLOCK(file);
	return true;
}

bool
File::moveRootPath(std::string newpath)
{
	scoped_lock<interprocess_upgradable_mutex> lock(rwl_file);
	string old_path = rootpath + filename;
	string new_path = newpath + filename;
	try {
		makePath(new_path);
	} catch (FileException e) {
		return false;
	}
	if (::rename(old_path.c_str(), new_path.c_str()) < 0)
		return false;
	rootpath = newpath;
	return true;
}

void
File::makePath(std::string path)
{
	size_t offset = 0;
	struct stat st;

	while (true) {
		size_t pos = path.find('/', offset);
		if (pos == string::npos)
			break;
		std::string prefix = path.substr(0, pos);
		if (stat(prefix.c_str(), &st) < 0)
			if (mkdir(prefix.c_str(), 0755) < 0)
				throw FileException("cannot create prefix path '" + prefix + "' to cover entire path '" + path + "'");
		offset = pos + 1;
	}
}

/* vim:set ts=2 sw=2: */
