#include <boost/thread/shared_mutex.hpp>
#include <list>
#include "file.h"

#ifndef __TORTILLA_FILEMANAGER_H__
#define __TORTILLA_FILEMANAGER_H__

namespace Tortilla {

class Overseer;

/*! \brief Responsible for handeling the pool of torrent files
 *
 *  Upon downloading torrents with a lot of files, we may run out of file
 *  descriptors. This object ensures we have an upper bound to the number
 *  of files in use, and that needless files are closed.
 */
class FileManager {
public:
	/*! \brief Constructs a new file manager
	 *  \param o Overseer object to use
	 *  \param max Number of open files at once
	 */
	FileManager(Overseer* o, unsigned int max);

	/*! \brief Destroys the file manager
	 *
	 *  This will not close any files managed; this is up to the torrent.
	 */
	~FileManager();

	//! \brief Adds a file to the manager
	void addFile(File* f);

	//! \brief Removes a file
	void removeFile(File* f);

	//! \brief Write to a file
	void writeFile(File* f, off_t offset, const void* buf, size_t len);

	//! \brief Read from a file
	void readFile(File* f, off_t offset, void* buf, size_t len);

	/*! \brief Sets the maximum number of files that will be opened
	 *  \param max New maximum number
	 */
	void setMaxOpenFiles(int max);

protected:
	/*! \brief Used to close unused files
	 *  \param f The file we are cleaning up for
	 *
	 *  Using a value of NULL for f means we aren't cleaning up
	 *  for a specific file, and are just intending to update
	 *  for a possible new value of maxFiles.
	 */
	void cleanup(File* f = NULL);

	/*! \brief Ensures the file is usuable for reading/writing
	 *
	 *  Note that this must be called with a locked File.
	 */
	void prepare(File* f);

private:
	//! \brief Our overseer object
	Overseer* overseer;

	//! \brief Current number of open files
	unsigned int curFiles;

	//! \brief Maximum number of open files at any time
	unsigned int maxFiles;

	//! \brief Files managed by us
	std::list<File*> files;

	//! \brief Lock used to protect our data fields
	boost::shared_mutex rwl_data;
};

}

#endif /* __TORTILLA_FILEMANAGER_H__ */
