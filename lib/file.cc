#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include "exceptions.h"
#include "file.h"
#include "macros.h"

using namespace std;

File::File(std::string path, off_t len)
{
	filename = path; length = len; reopened = false; lastInteraction = time(NULL);
	INIT_RWLOCK(file);

	/*
	 * First of all, try to open the file; if this works, we know the
	 * file pre-existed and we should refetch all of it (hopefully)
	 */
	if ((fd = ::open(path.c_str(), O_RDWR)) < 0) {
		/* This failed; attempt to create the file */
		if ((fd = ::open(path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644)) < 0)
			throw FileException("unable to create '" + path + "'");
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
	assert(offset + len <= length);
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
	assert(offset + len <= length);
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
	WLOCK(file);
	close();
	
	DESTROY_RWLOCK(file);
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

	if ((fd = ::open(filename.c_str(), O_RDWR)) < 0) {
		/*
		 * If we couldn't re-open the file, that's weird. We could do it in the
	 	 * constructor...
		 */
		throw FileException("unable to re-open '" + filename + "'");
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
}

void
File::lockWrite()
{
	WLOCK(file);
}

void
File::unlock()
{
	RWUNLOCK(file);
}

bool
File::compareByLastInteraction(File* a, File* b)
{
	return a->getLastInteraction() < b->getLastInteraction();
}

/* vim:set ts=2 sw=2: */
