#include <assert.h>
#include <iostream>
#include <stdint.h>
#include "sha1.h"

using namespace std;

HashSHA1::HashSHA1()
	: computed(false)
{
	SHA1_Init(&ctx); 
}

void
HashSHA1::process(const void* buf, size_t size)
{
	assert(!computed);
	SHA1_Update(&ctx, buf, size);
}

const uint8_t*
HashSHA1::getHash()
{
	if (computed)
		return hash;

	/* Retrieve the hash */
	SHA1_Final(hash, &ctx);
	computed = true;
	return hash;
}

std::string
HashSHA1::getHashAsASCII(const uint8_t* hash)
{
	string result = "";

	for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
		for (int j = 1; j >= 0; j--)
			result += "0123456789abcdef"[(hash[i] >> j * 4) & 0xf];
	return result;
}

/* vim:set ts=2 sw=2: */
