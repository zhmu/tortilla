#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include "exceptions.h"
#include "file.h"

using namespace std;

File::File(std::string path, size_t len)
{
	/*
	 * First of all, try to open the file; if this works, we know the
	 * file pre-existed and we should refetch all of it (hopefully)
	 */
	if ((writeFD = open(path.c_str(), O_WRONLY)) < 0) {
		/* This failed; attempt to reopen the file */
		if ((writeFD = creat(path.c_str(), 0644)) < 0)
			throw FileException("unable to create '" + path + "'");
		reopened = false;
	} else {
		reopened = true;
	}
	if ((readFD = open(path.c_str(), O_RDONLY)) < 0)
		throw FileException("unable to reopen '" + path + "'");

	if (reopened) {
		/* We reopened the file, but maybe the length is invalid */
		size_t filesize = lseek(writeFD, 0, SEEK_END);
		if (filesize == len) {
			/* all done!*/
			length = len;
			return;
		}

		/*
		 * File size mismatch - throw the file away and recreate it. We
		 * us an offset of 0 as this seems the safest bet; we enlarge
		 * the file to what we want below.
		 */
		ftruncate(writeFD, 0);
		reopened = false;
	}

	/*
	 * Expand the file to the requested length by seeking almost to the
	 * end and writing a byte there.
	 */
	uint8_t b = 0xFF;
	lseek(writeFD, len - 1, SEEK_SET);
	if (!::write(writeFD, &b, 1))
		throw FileException("unable to expand file");
	length = len;
}
	
void
File::write(size_t offset, const void* buf, size_t len)
{
	assert(offset + len <= length);

	lseek(writeFD, offset, SEEK_SET);
	if (::write(writeFD, buf, len) != len)
		throw FileException("short write");
}

void
File::read(size_t offset, void* buf, size_t len)
{
	assert(offset + len <= length);

	lseek(readFD, offset, SEEK_SET);
	if (::read(readFD, buf, len) != len)
		throw FileException("short read");
}

File::~File()
{
	close(readFD);
	close(writeFD);
}
