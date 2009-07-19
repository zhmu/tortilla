#include <pthread.h>
#include <string>
#include <time.h>

#ifndef __TORTILLA_FILE_H__
#define __TORTILLA_FILE_H__

class FileManager;

//! \brief Implements a basic file suitable for reading/writing
class File {
friend class FileManager;
public:
	/*! \brief Construct a new file
	 *  \param path Path to use
	 *  \param len Length of the file
	 */
	File(std::string path, off_t len);

	//! \brief Closes the file
	~File();

	/*! \brief Write a piece to the file
	 *  \param offset Byte offset where to write
	 *  \param buf Buffer to write
	 *  \param len Number of bytes to write
	 */
	void write(off_t offset, const void* buf, size_t len);

	/*! \brief Read a piece from the file
	 *  \param offset Byte offset from which to read 
	 *  \param buf Buffer to read to
	 *  \param len Number of bytes to read
	 */
	void read(off_t offset, void* buf, size_t len);

	//! \brief Retrieve the file length
	off_t getLength() { return length; }

	//! \brief Have we opened a preexisting file?
	bool haveReopened() { return reopened; }

	//! \brief Retrieve the file name
	std::string getFilename() { return filename; }

	//! \brief Retrieves last interaction timestamp
	time_t getLastInteraction() { return lastInteraction; }

	//! \brief Compares two files based on last interaction timestamp
	static bool compareByLastInteraction(File* a, File* b);

protected:
	//! \brief Open the file
	void open();

	//! \brief Close the file
	void close();

	//! \brief Is the file opened?
	bool isOpened();

	//! \brief Locks the file object for read
	void lockRead();

	//! \brief Locks the file object for write
	void lockWrite();

	//! \brief Unlocks the file object
	void unlock();

private:
	//! \brief Length of the file
	off_t length;

	//! \brief File descriptor
	int fd;

	//! \brief Have we re-opened a previous file?
	bool reopened;

	//! \brief Name of the file
	std::string filename;

	//! \brief Timestamp of the last interaction
	time_t lastInteraction;

	//! \brief Mutex used to guard the file from open/close-ing
	pthread_rwlock_t rwl_file;
};

#endif /*  __TORTILLA_FILE_H__ */
