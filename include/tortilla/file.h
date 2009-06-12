#include <pthread.h>
#include <string>

#ifndef __FILE_H__
#define __FILE_H__

//! \brief Implements a basic file suitable for reading/writing
class File {
public:
	/*! \brief Construct a new file
	 *  \param path Path to use
	 *  \param len Length of the file
	 */
	File(std::string path, size_t len);

	//! \brief Closes the file
	~File();

	/*! \brief Write a piece to the file
	 *  \param offset Byte offset where to write
	 *  \param buf Buffer to write
	 *  \param len Number of bytes to write
	 */
	void write(size_t offset, const void* buf, size_t len);

	/*! \brief Read a piece from the file
	 *  \param offset Byte offset from which to read 
	 *  \param buf Buffer to read to
	 *  \param len Number of bytes to read
	 */
	void read(size_t offset, void* buf, size_t len);

	//! \brief Retrieve the file length
	size_t getLength() { return length; }

	//! \brief Have we opened a preexisting file?
	bool haveReopened() { return reopened; }

	//! \brief Retrieve the file name
	std::string getFilename() { return filename; }

private:
	//! \brief Length of the file
	size_t length;

	//! \brief File descriptor
	int fd;

	//! \brief Have we re-opened a previous file?
	bool reopened;

	//! \brief Name of the file
	std::string filename;
};

#endif /*  __FILE_H__ */
