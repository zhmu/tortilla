#include <boost/thread/shared_mutex.hpp>
#include <string>
#include <time.h>

#ifndef __TORTILLA_FILE_H__
#define __TORTILLA_FILE_H__

namespace Tortilla {

class FileManager;

//! \brief Implements a basic file suitable for reading/writing
class File {
friend class FileManager;
public:
	/*! \brief Construct a new file
	 *  \param path Path to use
	 *  \param len Length of the file
	 *  \param root_path Root directory of the file path
	 *
	 *  The full filename is root_path + path, without any '/'. This is
	 *  useful for moving the file in place.
	 */
	File(std::string path, off_t len, std::string root_path);

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
	off_t getLength() const { return length; }

	//! \brief Have we opened a preexisting file?
	bool haveReopened() const { return reopened; }

	//! \brief Retrieve the file name
	const std::string& getFilename() const { return filename; }

	//! \brief Retrieves last interaction timestamp
	time_t getLastInteraction() const { return lastInteraction; }

	//! \brief Compares two files based on last interaction timestamp
	static bool compareByLastInteraction(File* a, File* b);

	/*! \brief Rename the file
	 *  \param newpath New path of the file
	 *  \returns true if the rename was successful
	 *
	 *  Upon success, the filename in the object will also be updated.
	 */
	bool rename(std::string newpath);

	//! \brief Retrieve the root path
	const std::string& getRootPath() const { return rootpath; }

	/*! \brief Move the file to a new root path
	 *  \param newpath New root path to use
	 *  \returns true on success
	 */
	bool moveRootPath(std::string newpath);

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

	/*! \brief Creates a directory structure for a pathname
	 *  \param pathname The path that will be used
	 *
	 *  This will ensure that pathname can be stored under the current root;
	 *  if 'pathname' is 'a/b/c/d.bin', it will ensure 'a', 'a/b' and 'a/b/c'
	 *  exist once this function returns.
	 */
	void makePath(std::string path);

private:
	//! \brief Length of the file
	off_t length;

	//! \brief File descriptor
	int fd;

	//! \brief Have we re-opened a previous file?
	bool reopened;

	//! \brief Name of the file
	std::string filename;

	//! \brief Root path where the file is
	std::string rootpath;

	//! \brief Timestamp of the last interaction
	time_t lastInteraction;

	//! \brief Mutex used to guard the file from open/close-ing
	boost::shared_mutex rwl_file;

	//! \brief Are we locked for reading?
	bool read_locked;
};

}

#endif /*  __TORTILLA_FILE_H__ */
