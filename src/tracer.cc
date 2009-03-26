#include <stdarg.h>
#include <time.h>
#include "tracer.h"

Tracer::Tracer()
{
	pthread_mutex_init(&mtx, NULL);
	tracefile = fopen("trace.log", "wt");
	tracerMask = 0xff;
}

Tracer::~Tracer()
{
	pthread_mutex_destroy(&mtx);
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

	pthread_mutex_lock(&mtx);
	fprintf(tracefile, "%s ", timestamp);
	vfprintf(tracefile, msg, vl);
	fprintf(tracefile, "\n");
	fflush(tracefile);
	pthread_mutex_unlock(&mtx);

	va_end(vl);
}

/* vim:set ts=2 sw=2: */
