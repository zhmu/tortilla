#ifndef __MACROS_H__
#define __MACROS_H__

#define MIN(a,b) \
	(((a) < (b)) ? (a) : (b))

#define LOCK(x)     pthread_mutex_lock(&mtx_ ## x);
#define UNLOCK(x)   pthread_mutex_unlock(&mtx_ ## x);
#define RLOCK(x)    pthread_rwlock_rdlock(&rwl_## x);
#define WLOCK(x)    pthread_rwlock_wrlock(&rwl_## x);
#define RWUNLOCK(x) pthread_rwlock_unlock(&rwl_## x);

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


#endif /* __MACROS_H__ */
