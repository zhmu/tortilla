#include <err.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <time.h>
#include "tortilla/sha1.h"

static const int CHUNK_SIZE = 4096;

std::string
hash2ascii(const uint8_t* hash)
{
	std::string s;
	for (int i = 0; i < 20; i++) {
		char tmp[16];
		sprintf(tmp, "%02x", hash[i]);
		s += tmp;
	}
	return s;
}

float
time_diff(const struct timespec* a, const struct timespec* b)
{
	float diff = (b->tv_sec - a->tv_sec) * 1000000000.0f;
	if (b->tv_nsec < a->tv_nsec) /* wrap */ {
		diff -=  1000000000;
		diff += (1000000000 - a->tv_nsec) + b->tv_nsec;
	} else {
		diff += b->tv_nsec - a->tv_nsec;
	}
	return diff;
}

int
main(int argc, char* argv[])
{
	if (argc != 2)
		errx(1, "usage: need a filename");

	FILE* f = fopen(argv[1], "rb");
	if (f == NULL)
		err(1, "unable to open file");
	fseek(f, 0, SEEK_END);
	unsigned long l = ftell(f);
	char* ptr = (char*)malloc(l);
	if (ptr == NULL)
		err(1, "cannot allocate memory");
	rewind(f);
	struct timespec begin_time;
	fprintf(stderr, "\rreading %lu bytes...", l);
	clock_gettime(CLOCK_REALTIME, &begin_time);
	for (unsigned long pos = 0; pos < l; /* nothing */) {
		unsigned int chunk_len = std::min(l - pos, (unsigned long)CHUNK_SIZE);
		if (chunk_len == 0)
			break;
		if (!fread((void*)(ptr + pos), chunk_len, 1, f))
			err(1, "read error");
		pos += chunk_len;
	}
	fclose(f);
	struct timespec end_time;
	clock_gettime(CLOCK_REALTIME, &end_time);
	fprintf(stderr, "\rreading %lu bytes... done, %.2f KB/sec\n", l, ((float)l / 1024.f) / (time_diff(&begin_time, &end_time) / 1000000000.f));

	{
		fprintf(stderr, "hashing using own implementation    : ");
		struct timespec cur_time, done_time;
		clock_gettime(CLOCK_REALTIME, &cur_time);
		Tortilla::HashSHA1 hash;
		for (unsigned long pos = 0; pos < l; /* nothing */) {
			unsigned int chunk_len = std::min(l - pos, (unsigned long)CHUNK_SIZE);
			if (chunk_len == 0)
				break;
			hash.process((char*)(ptr + pos), chunk_len);
			pos += chunk_len;
		}
		const uint8_t* hashval = hash.getHash();
		clock_gettime(CLOCK_REALTIME, &done_time);
		fprintf(stderr, " ok, %s, ~%f ms\n", hash2ascii(hashval).c_str(), time_diff(&cur_time, &done_time) / 1000000.f);
	}
	{
		fprintf(stderr, "hashing using openssl implementation: ");
		struct timespec cur_time, done_time;
		clock_gettime(CLOCK_REALTIME, &cur_time);
		SHA_CTX ctx;
		SHA1_Init(&ctx);
		for (unsigned long pos = 0; pos < l; /* nothing */) {
			unsigned int chunk_len = std::min(l - pos, (unsigned long)CHUNK_SIZE);
			if (chunk_len == 0)
				break;
			SHA1_Update(&ctx, (char*)(ptr + pos), chunk_len);
			pos += chunk_len;
		}
		uint8_t hash[20];
		SHA1_Final(hash, &ctx);
		clock_gettime(CLOCK_REALTIME, &done_time);
		fprintf(stderr, " ok, %s, ~%f ms\n", hash2ascii(hash).c_str(), time_diff(&cur_time, &done_time) / 1000000.f);
	}

	free(ptr);
	return 0;
}

/* vim:set ts=2 sw=2: */
