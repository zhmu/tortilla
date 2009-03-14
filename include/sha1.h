#include <openssl/sha.h>
#include <string>

#ifndef __SHA1_H__
#define __SHA1_H__

class HashSHA1 {
public:
	//! \brief Constructs a new SHA1 hasher
	HashSHA1();

	/*! \brief Process a piece of data
	 *  \param buf Buffer to process
	 *  \param size Number of bytes to process
	 */
	void process(const void* buf, size_t size);
	
	/*! \brief Retrieves the hash for the input stream
	 *
	 *  The hash will be calculated here if needed.
	 */
	std::string getHash();

	//! \brief Retrieves the hash as an ASCII string
	std::string getHashAsASCII();

	//! \brief Retrieves the chunk size used to calculate the hash
	size_t getChunkSize() { return 1024; }

private:
	//! \brief OpenSSL SHA contex
	SHA_CTX ctx;

	//! \brief Computed hash, if any
	std::string hash;
};

#endif /* __SHA1_H__ */
