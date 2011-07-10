#include <boost/thread/locks.hpp>
#include <algorithm>
#include <assert.h>
#include <list>
#include "exceptions.h"
#include "file.h"
#include "filemanager.h"
#include "macros.h"
#include "overseer.h"
#include "tracer.h"

using namespace std;
using namespace boost;

#define TRACER (overseer->getTracer())

FileManager::FileManager(Overseer* o, unsigned int max)
{
	assert(max > 0);

	overseer = o; curFiles = 0; maxFiles = max;
}

FileManager::~FileManager()
{
}

void
FileManager::writeFile(File* f, off_t offset, const void* buf, size_t len)
{
	f->lockWrite();
	prepare(f);
	f->write(offset, buf, len);
	f->unlock();
}

void
FileManager::readFile(File* f, off_t offset, void* buf, size_t len)
{
	f->lockRead();
	prepare(f);
	f->read(offset, buf, len);
	f->unlock();
}

void
FileManager::prepare(File* f)
{
	try {
		if (!f->isOpened()) {
			cleanup(f);
			f->open();
			{
				unique_lock<shared_mutex> lock(rwl_data);
				curFiles++;
			}
		}
	} catch (FileException e) {
		f->unlock(); /* don't leave file locked, this causes a deadlock */
		throw e;
	}
}

void
FileManager::cleanup(File* f)
{
	list<File*> sortedFiles;
	{
		shared_lock<shared_mutex> lock(rwl_data);
		unsigned int cur = curFiles;
		if (cur < maxFiles)
			return;
		sortedFiles = files;
	}
	sortedFiles.sort(File::compareByLastInteraction);

	/* Wade through the sorted file list, and get close the first possible file */
	for (list<File*>::iterator it = sortedFiles.begin();
		   it != sortedFiles.end(); it++) {
		File* file = *it;
		/* Skip the file we are cleaning up for; it is already locked by us */
		if (file == f)
			continue;

		/* XXX this may ignore/close too many files some of the files! */
		if (!file->isOpened())
			continue;

		/* This old file is open; close it */
		file->lockWrite();
		file->close();
		file->unlock();

		{
			unique_lock<shared_mutex> lock(rwl_data);
			curFiles--;
		}

		/* XXX exit immediately, or close more, say 10% of all files? */
		break;
	}
}

void
FileManager::addFile(File* f)
{
	unique_lock<shared_mutex> lock(rwl_data);
	files.push_back(f);
}

void
FileManager::removeFile(File* f)
{
	unique_lock<shared_mutex> lock(rwl_data);
	files.remove(f);
}

void
FileManager::setMaxOpenFiles(int max)
{
	{
		unique_lock<shared_mutex> lock(rwl_data);
		maxFiles = max;
	}

	/* ensure the new maximum is honored (XXX is this safe to call?) */
	cleanup();
}

/* vim:set ts=2 sw=2: */
