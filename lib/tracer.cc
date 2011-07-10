#include <boost/thread/locks.hpp>
#include <stdarg.h>
#include <time.h>
#include "macros.h"
#include "tracer.h"

using namespace boost;

Tracer::Tracer()
{
	tracefile = fopen("trace.log", "wt");
	tracerMask = 0xffff;
}

Tracer::~Tracer()
{
	if (tracefile != NULL)
		fclose(tracefile);
}

void
Tracer::trace(unsigned int type, const char* msg, ...)
{
	char timestamp[64 /* XXX */];
	struct tm tm;
	time_t t;

	va_list vl;
	if (!(tracerMask & type) || tracefile == NULL)
		return;

	time(&t);
	localtime_r(&t, &tm);
	strftime(timestamp, sizeof(timestamp), "%b %d %T", &tm);
	va_start(vl, msg);

	{
		unique_lock<mutex> lock(mtx_file);
		fprintf(tracefile, "%s ", timestamp);
		vfprintf(tracefile, msg, vl);
		fprintf(tracefile, "\n");
		fflush(tracefile);
	}

	va_end(vl);
}

/* vim:set ts=2 sw=2: */
