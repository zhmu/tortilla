/*
 * The HashSHA1 code is based on Paul E. Jones <paulej@packetizer.com>'s
 * freeware SHA1 code, obtainable from http://www.packetizer.com/security/sha1/
 * license text is as follows:
 *
 * Copyright (C) 1998, 2009
 * Paul E. Jones <paulej@packetizer.com>
 * 
 * Freeware Public License (FPL)
 * 
 * This software is licensed as "freeware."  Permission to distribute
 * this software in source and binary forms, including incorporation
 * into other products, is hereby granted without a fee.  THIS SOFTWARE
 * IS PROVIDED 'AS IS' AND WITHOUT ANY EXPRESSED OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE AUTHOR SHALL NOT BE HELD
 * LIABLE FOR ANY DAMAGES RESULTING FROM THE USE OF THIS SOFTWARE, EITHER
 * DIRECTLY OR INDIRECTLY, INCLUDING, BUT NOT LIMITED TO, LOSS OF DATA
 * OR DATA BEING RENDERED INACCURATE.
 *
 * It has been incorperated in the HashSHA1 class.
 */
#include <assert.h>
#include <iostream>
#include <stdint.h>
#include "sha1.h"

using namespace std;

HashSHA1::HashSHA1()
	: computed(false), Length_Low(0), Length_High(0), Message_Block_Index(0)
{
	H[0] = 0x67452301;
	H[1] = 0xEFCDAB89;
	H[2] = 0x98BADCFE;
	H[3] = 0x10325476;
	H[4] = 0xC3D2E1F0;
}

void
HashSHA1::process(const void* buf, size_t size)
{
	assert(!computed);

	const uint8_t* input = (const uint8_t*)buf;
	while(size--) {
		Message_Block[Message_Block_Index++] = (*input & 0xff);

		Length_Low += 8;
		Length_Low &= 0xffffffff;             // Force it to 32 bits
		if (Length_Low == 0) {
			Length_High++;
			Length_High &= 0xffffffff;          // Force it to 32 bits
			assert(Length_High != 0);           // Message is too long
		}

		if (Message_Block_Index == 64)
			ProcessMessageBlock();
		input++;
	}
}

const uint8_t*
HashSHA1::getHash()
{
	if (computed)
		return hash;

	/* Retrieve the hash */
	PadMessage();
	for(int i = 0; i < 5; i++) {
		hash[i * 4 + 0] =  H[i] >> 24;
		hash[i * 4 + 1] = (H[i] >> 16) & 0xff;
		hash[i * 4 + 2] = (H[i] >>  8) & 0xff;
		hash[i * 4 + 3] = (H[i] >>  0) & 0xff;
  }
	computed = true;
	return hash;
}

uint32_t
HashSHA1::CircularShift(int bits, uint32_t word)
{
    return ((word << bits) & 0xFFFFFFFF) | ((word & 0xFFFFFFFF) >> (32-bits));
}

void
HashSHA1::ProcessMessageBlock()
{
	// Constants defined for SHA-1
	const uint32_t K[] = { 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };
	uint32_t W[80]; // Word sequence

	//  Initialize the first 16 words in the array W
	for(int t = 0; t < 16; t++) {
		W[t] = ((unsigned) Message_Block[t * 4]) << 24;
		W[t] |= ((unsigned) Message_Block[t * 4 + 1]) << 16;
		W[t] |= ((unsigned) Message_Block[t * 4 + 2]) << 8;
		W[t] |= ((unsigned) Message_Block[t * 4 + 3]);
	}

	for(int t = 16; t < 80; t++) {
		W[t] = CircularShift(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16]);
	}

	uint32_t A = H[0];
	uint32_t B = H[1];
	uint32_t C = H[2];
	uint32_t D = H[3];
	uint32_t E = H[4];

	for(int t = 0; t < 20; t++) {
		uint32_t temp = CircularShift(5,A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0];
		E = D;
		D = C;
		C = CircularShift(30,B);
		B = A;
		A = temp;
	}

	for(int t = 20; t < 40; t++) {
		uint32_t temp = CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[1];
		E = D;
		D = C;
		C = CircularShift(30,B);
		B = A;
		A = temp;
	}

	for(int t = 40; t < 60; t++) {
		uint32_t temp = CircularShift(5,A) + ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2];
		E = D;
		D = C;
		C = CircularShift(30,B);
		B = A;
		A = temp;
	}

	for(int t = 60; t < 80; t++) {
		uint32_t temp = CircularShift(5,A) + (B ^ C ^ D) + E + W[t] + K[3];
		E = D;
		D = C;
		C = CircularShift(30,B);
		B = A;
		A = temp;
	}

	H[0] = (H[0] + A);
	H[1] = (H[1] + B);
	H[2] = (H[2] + C);
	H[3] = (H[3] + D);
	H[4] = (H[4] + E);

	Message_Block_Index = 0;
}

void
HashSHA1::PadMessage()
{
	/*
	 *  Check to see if the current message block is too small to hold
	 *  the initial padding bits and length.  If so, we will pad the
	 *  block, process it, and then continue padding into a second block.
	 */
	if (Message_Block_Index > 55) {
		Message_Block[Message_Block_Index++] = 0x80;
		while(Message_Block_Index < 64) {
			Message_Block[Message_Block_Index++] = 0;
		}

		ProcessMessageBlock();

		while(Message_Block_Index < 56) {
			Message_Block[Message_Block_Index++] = 0;
		}
	} else {
		Message_Block[Message_Block_Index++] = 0x80;
		while(Message_Block_Index < 56) {
			Message_Block[Message_Block_Index++] = 0;
		}
	}

	/*
	 *  Store the message length as the last 8 octets
	 */
	Message_Block[56] = (Length_High >> 24) & 0xFF;
	Message_Block[57] = (Length_High >> 16) & 0xFF;
	Message_Block[58] = (Length_High >> 8) & 0xFF;
	Message_Block[59] = (Length_High) & 0xFF;
	Message_Block[60] = (Length_Low >> 24) & 0xFF;
	Message_Block[61] = (Length_Low >> 16) & 0xFF;
	Message_Block[62] = (Length_Low >> 8) & 0xFF;
	Message_Block[63] = (Length_Low) & 0xFF;

	ProcessMessageBlock();
}

/* vim:set ts=2 sw=2: */
