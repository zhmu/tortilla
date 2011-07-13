#include <stdint.h>
#include <string>

#ifndef __TORTILLA_SHA1_H__
#define __TORTILLA_SHA1_H__

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

	//! \brief Retrieves the chunk size used to calculate the hash
	static size_t getChunkSize() { return 1024; }

private:
	//! \brief Length of a SHA1 hash
	static const int SHA1_DIGEST_LENGTH = 20;

	//! \brief Have we computed the hash
	bool computed;

	//! \brief Computed hash, if any
	uint8_t hash[SHA1_DIGEST_LENGTH];

        //! \brief *Process the next 512 bits of the message
        void ProcessMessageBlock();

        //!\ \brief  Pads the current message block to 512 bits
        void PadMessage();

        /*
         *  Performs a circular left shift operation
         */
        inline unsigned CircularShift(int bits, unsigned word);

        unsigned H[5];                      // Message digest buffers

        unsigned Length_Low;                // Message length in bits
        unsigned Length_High;               // Message length in bits

        unsigned char Message_Block[64];    // 512-bit message blocks
        int Message_Block_Index;            // Index into message block array

        bool Computed;                      // Is the digest computed?
        bool Corrupted;                     // Is the message digest corruped?
};

#endif /* __TORTILLA_SHA1_H__ */
