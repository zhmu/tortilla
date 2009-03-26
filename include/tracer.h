#include <pthread.h>
#include <stdio.h>

#ifndef __TRACER_H__
#define __TRACER_H__

//! \brief Trace network connect/disconnect events
#define TRACER_TYPE_NETWORK	0x0001

//! \brief Trace hi-level torrent behaviour
#define TRACER_TYPE_TORRENT	0x0002

//! \brief Trace BitTorrent protocol messages
#define TRACER_TYPE_PROTOCOL	0x0004

//! \brief Trace hasher events
#define TRACER_TYPE_HASHER	0x0008

//! \brief Trace tracker events
#define TRACER_TYPE_TRACKER	0x0010

//! \brief Trace choking algorithm
#define TRACER_TYPE_CHOKING	0x0020

/*! \brief Handles tracing of events for debugging purposes
 */
class Tracer {
public:
	//! \brief Constract a debugging tracer
	Tracer();

	//! \brief Destruct the tracer
	~Tracer();

	//! \brief Trace an event
	void trace(unsigned int type, const char* msg, ...);

private:
	//! \brief File we are tracing to
	FILE* tracefile;

	//! \brief Mask of events we are tracing
	unsigned int tracerMask;

	//! \brief Mutex ensuring we don't write between writes
	pthread_mutex_t mtx;
};

extern Tracer* tracer;

#define TRACE(t,format,args...) \
	tracer->trace(TRACER_TYPE_ ## t, format, ## args)

#endif /* __TRACER_H__ */
