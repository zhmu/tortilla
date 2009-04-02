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
	const uint8_t* getHash();

	/*! \brief Retrieves a hash as an ASCII string
	 *  \param hash Hash to convert
	 */
	std::string getHashAsASCII(const uint8_t* hash);

	//! \brief Retrieves the chunk size used to calculate the hash
	size_t getChunkSize() { return 1024; }

private:
	//! \brief OpenSSL SHA contex
	SHA_CTX ctx;

	//! \brief Have we computed the hash
	bool computed;

	//! \brief Computed hash, if any
	uint8_t hash[SHA_DIGEST_LENGTH];
};

#endif /* __SHA1_H__ */