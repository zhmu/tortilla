#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include "exceptions.h"
#include "file.h"
#include "macros.h"

using namespace std;

File::File(std::string path, size_t len)
{
	filename = path; length = len;

	/*
	 * First of all, try to open the file; if this works, we know the
	 * file pre-existed and we should refetch all of it (hopefully)
	 */
	if ((fd = open(path.c_str(), O_RDWR)) < 0) {
		/* This failed; attempt to create the file */
		if ((fd = open(path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0644)) < 0)
			throw FileException("unable to create '" + path + "'");
		reopened = false;
	} else {
		reopened = true;
	}

	if (reopened) {
		/* We reopened the file, but maybe the length is invalid */
		size_t filesize = lseek(fd, 0, SEEK_END);
		if (filesize == len) {
			/* all done!*/
			return;
		}

		/*
		 * File size mismatch - throw the file away and recreate it. We
		 * us an offset of 0 as this seems the safest bet; we enlarge
		 * the file to what we want below.
		 */
		ftruncate(fd, 0);
		reopened = false;
	}

	/*
	 * Expand the file to the requested length by seeking almost to the
	 * end and writing a byte there.
	 */
	uint8_t b = 0xFF;
	lseek(fd, len - 1, SEEK_SET);
	if (!::write(fd, &b, 1))
		throw FileException("unable to expand file");
}
	
void
File::write(size_t offset, const void* buf, size_t len)
{
	assert(offset + len <= length);

	/*
	 * Don't consider short writes a failure if the call was interrupted;
	 * this happens if the user hits ^C to exit.
	 */
	if ((size_t)pwrite(fd, buf, len, offset) != len && errno != EINTR)
		throw FileException("short write");
}

void
File::read(size_t offset, void* buf, size_t len)
{
	assert(offset + len <= length);

	/*
	 * Don't consider short writes a failure if the call was interrupted;
	 * this happens if the user hits ^C to exit.
	 */
	if ((size_t)pread(fd, buf, len, offset) != len && errno != EINTR)
		throw FileException("short read");
}

File::~File()
{
	close(fd);
}

/* vim:set ts=2 sw=2: */
