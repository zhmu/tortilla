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

#define TRACER (overseer->getTracer())

FileManager::FileManager(Overseer* o, unsigned int max)
{
	assert(max > 0);

	INIT_RWLOCK(data);
	overseer = o; curFiles = 0; maxFiles = max;
}

FileManager::~FileManager()
{
	DESTROY_RWLOCK(data);
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
			WLOCK(data);
			curFiles++;
			RWUNLOCK(data);
		}
	} catch (FileException e) {
		f->unlock(); /* don't leave file locked, this causes a deadlock */
		throw e;
	}
}

void
FileManager::cleanup(File* f)
{
	RLOCK(data);
	unsigned int cur = curFiles;
	if (cur < maxFiles) {
		RWUNLOCK(data);
		return;
	}
	list<File*> sortedFiles = files;
	sortedFiles.sort(File::compareByLastInteraction);
	RWUNLOCK(data);

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

		WLOCK(data);
		curFiles--;
		RWUNLOCK(data);

		/* XXX exit immediately, or close more, say 10% of all files? */
		break;
	}
}

void
FileManager::addFile(File* f)
{
	WLOCK(data);
	files.push_back(f);
	RWUNLOCK(data);
}

void
FileManager::removeFile(File* f)
{
	WLOCK(data);
	files.remove(f);
	RWUNLOCK(data);
}


/* vim:set ts=2 sw=2: */
