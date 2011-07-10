#ifndef __TORTILLA_MACROS_H__
#define __TORTILLA_MACROS_H__

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
