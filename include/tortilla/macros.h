#ifndef __TORTILLA_MACROS_H__
#define __TORTILLA_MACROS_H__

#define MIN(a,b) \
	(((a) < (b)) ? (a) : (b))

#define STRINGIFY(x) XSTRINGIFY(x)
#define XSTRINGIFY(x) #x

#ifdef DEBUG_LOCKING
#define LOCK_DEBUG_PRINTF(fmt,args...) \
	fprintf(stderr, fmt, ## args); \
	fflush(stderr);
#else
#define LOCK_DEBUG_PRINTF(fmt,args...)
#endif

#define INIT_MUTEX(x) \
do { \
	LOCK_DEBUG_PRINTF("INIT_MUTEX(%s:%u,thread=%p,mutex=%s)\n", __FILE__, __LINE__, pthread_self(), "mtx_" STRINGIFY(x)); \
	pthread_mutex_init(&mtx_ ## x, NULL); \
} while(0);

#define DESTROY_MUTEX(x) \
do { \
	LOCK_DEBUG_PRINTF("DESTROY_MUTEX(%s:%u,thread=%p,mutex=%s)\n", __FILE__, __LINE__, pthread_self(), "mtx_" STRINGIFY(x)); \
	pthread_mutex_destroy(&mtx_ ## x); \
} while(0);

#define LOCK(x) \
do { \
	LOCK_DEBUG_PRINTF("LOCK(%s:%u,thread=%p,mutex=%s)\n", __FILE__, __LINE__, pthread_self(), "mtx_" STRINGIFY(x)); \
	pthread_mutex_lock(&mtx_ ## x); \
} while (0);

#define UNLOCK(x) \
do { \
	LOCK_DEBUG_PRINTF("UNLOCK(%s:%u,thread=%p,mutex=%s)\n", __FILE__, __LINE__, pthread_self(), "mtx_" STRINGIFY(x)); \
	pthread_mutex_unlock(&mtx_ ## x); \
} while(0);

#define INIT_RWLOCK(x) \
do { \
	LOCK_DEBUG_PRINTF("INIT_RWLOCK(%s:%u,thread=%p,rwlock=%s)\n", __FILE__, __LINE__, pthread_self(), "rwl_" STRINGIFY(x)); \
	pthread_rwlock_init(&rwl_## x, NULL); \
} while(0);

#define DESTROY_RWLOCK(x) \
do { \
	LOCK_DEBUG_PRINTF("DESTROY_RWLOCK(%s:%u,thread=%p,rwlock=%s)\n", __FILE__, __LINE__, pthread_self(), "rwl_" STRINGIFY(x)); \
	pthread_rwlock_destroy(&rwl_ ## x); \
} while (0);

#define RLOCK(x) \
do { \
	LOCK_DEBUG_PRINTF("RLOCK(%s:%u,thread=%p,rwlock=%s)\n", __FILE__, __LINE__, pthread_self(), "rwl_" STRINGIFY(x)); \
	pthread_rwlock_rdlock(&rwl_## x); \
} while(0);

#define WLOCK(x) \
do { \
	LOCK_DEBUG_PRINTF("WLOCK(%s:%u,thread=%p,rwlock=%s)\n", __FILE__, __LINE__, pthread_self(), "rwl_" STRINGIFY(x)); \
	pthread_rwlock_wrlock(&rwl_## x); \
} while(0);

#define RWUNLOCK(x) \
do { \
	LOCK_DEBUG_PRINTF("RWUNLOCK(%s:%u,thread=%p,rwlock=%s)\n", __FILE__, __LINE__, pthread_self(), "rwl_" STRINGIFY(x)); \
	pthread_rwlock_unlock(&rwl_## x); \
} while(0);

#define WRITE_UINT32(ptr,offs,val) \
	ptr[(offs) + 0] = (((val) >> 24) & 0xff); \
	ptr[(offs) + 1] = (((val) >> 16) & 0xff); \
	ptr[(offs) + 2] = (((val) >>  8) & 0xff); \
	ptr[(offs) + 3] = (((val)      ) & 0xff);

#define READ_UINT32(ptr,offs) \
	(uint32_t)((((ptr)[(offs) + 0]) << 24) | \
	           (((ptr)[(offs) + 1]) << 16) | \
	           (((ptr)[(offs) + 2]) <<  8) | \
	           (((ptr)[(offs) + 3])      ))


#endif /* __TORTILLA_MACROS_H__ */
