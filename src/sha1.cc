#include <assert.h>
#include <iostream>
#include <stdint.h>
#include "sha1.h"

using namespace std;

HashSHA1::HashSHA1()
	: hash("")
{
	SHA1_Init(&ctx); 
}

void
HashSHA1::process(const void* buf, size_t size)
{
	assert(hash == "");
	SHA1_Update(&ctx, buf, size);
}

std::string
HashSHA1::getHash()
{
	if (hash != "")
		return hash;

	/* Retrieve the hash */
	unsigned char md[SHA_DIGEST_LENGTH + 1];
	md[SHA_DIGEST_LENGTH] = '\0';
	SHA1_Final(md, &ctx);

	hash = string((const char*)md);
	return hash;
}

std::string
HashSHA1::getHashAsASCII()
{
	string result = "";

	string hash = getHash();
	for (string::iterator it = hash.begin();
	     it != hash.end(); it++) {
		uint8_t b = (uint8_t)*it;
		for (int i = 1; i >= 0; i--)
			result += "0123456789abcdef"[(b >> i * 4) & 0xf];
	}
	return result;
}

/* vim:set ts=2 sw=2: */
