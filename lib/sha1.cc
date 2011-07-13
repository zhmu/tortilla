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
using namespace Tortilla;

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
	static const uint32_t K[] = { 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xCA62C1D6 };
	uint32_t W[80]; // Word sequence

	//  Initialize the first 16 words in the array W
	for(int t = 0; t < 16; t++) {
		W[t] = ((unsigned) Message_Block[t * 4]) << 24;
		W[t] |= ((unsigned) Message_Block[t * 4 + 1]) << 16;
		W[t] |= ((unsigned) Message_Block[t * 4 + 2]) << 8;
		W[t] |= ((unsigned) Message_Block[t * 4 + 3]);
	}

#define ROTATE(bits, word) \
	(((word) << (bits)) | (((word) >> (32-(bits)))))

#define S1(t) \
		W[t] = ROTATE(1,W[t-3] ^ W[t-8] ^ W[t-14] ^ W[t-16])

	S1(16); S1(17); S1(18); S1(19); S1(20); S1(21); S1(22); S1(23); S1(24);
	S1(25); S1(26); S1(27); S1(28); S1(29); S1(30); S1(31); S1(32); S1(33);
	S1(34); S1(35); S1(36); S1(37); S1(38); S1(39); S1(40); S1(41); S1(42);
	S1(43); S1(44); S1(45); S1(46); S1(47); S1(48); S1(49); S1(50); S1(51);
	S1(52); S1(53); S1(54); S1(55); S1(56); S1(57); S1(58); S1(59); S1(60);
	S1(61); S1(62); S1(63); S1(64); S1(65); S1(66); S1(67); S1(68); S1(69);
	S1(70); S1(71); S1(72); S1(73); S1(74); S1(75); S1(76); S1(77); S1(78);
	S1(79);

	register uint32_t A = H[0];
	register uint32_t B = H[1];
	register uint32_t C = H[2];
	register uint32_t D = H[3];
	register uint32_t E = H[4];

	register uint32_t temp;

#define S2(t) \
		temp = ROTATE(5,A) + ((B & C) | ((~B) & D)) + E + W[t] + K[0]; \
		E = D; \
		D = C; \
		C = ROTATE(30,B); \
		B = A; \
		A = temp;

	S2( 0); S2( 1); S2( 2); S2( 3); S2( 4); S2( 5); S2( 6); S2( 7); S2( 8); S2( 9);
	S2(10); S2(11); S2(12); S2(13); S2(14); S2(15); S2(16); S2(17); S2(18); S2(19);

#define S3(t) \
		temp = ROTATE(5,A) + (B ^ C ^ D) + E + W[t] + K[1]; \
		E = D; \
		D = C; \
		C = ROTATE(30,B); \
		B = A; \
		A = temp;

	S3(20); S3(21); S3(22); S3(23); S3(24); S3(25); S3(26); S3(27); S3(28); S3(29);
	S3(30); S3(31); S3(32); S3(33); S3(34); S3(35); S3(36); S3(37); S3(38); S3(39);

#define S4(t) \
		temp = ROTATE(5,A) + ((B & C) | (B & D) | (C & D)) + E + W[t] + K[2]; \
		E = D; \
		D = C; \
		C = ROTATE(30,B); \
		B = A; \
		A = temp;

	S4(40); S4(41); S4(42); S4(43); S4(44); S4(45); S4(46); S4(47); S4(48); S4(49);
	S4(50); S4(51); S4(52); S4(53); S4(54); S4(55); S4(56); S4(57); S4(58); S4(59);

#define S5(t) \
		temp = ROTATE(5,A) + (B ^ C ^ D) + E + W[t] + K[3]; \
		E = D; \
		D = C; \
		C = ROTATE(30,B); \
		B = A; \
		A = temp;

	S5(60); S5(61); S5(62); S5(63); S5(64); S5(65); S5(66); S5(67); S5(68); S5(69);
	S5(70); S5(71); S5(72); S5(73); S5(74); S5(75); S5(76); S5(77); S5(78); S5(79);

	H[0] += A;
	H[1] += B;
	H[2] += C;
	H[3] += D;
	H[4] += E;

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
