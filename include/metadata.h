#include <list>
#include <stdint.h>
#include <iostream>
#include <string>
#include "metafield.h"

#ifndef __METADATA_H__
#define __METADATA_H__

#define METADATA_CODE_UNKNOWN	0
#define METADATA_CODE_EOF	1
#define METADATA_CODE_CORRUPT	2

class MetaDataException {
public:
	MetaDataException(int code) {
		errorcode = code;
	}

private:
	int errorcode;
};

/*! \brief Contains torrent file metadata
 */
class Metadata {
public:
	/*! \brief Constructs a new metadata object
	 *  \param s Stream to construct the metadata from
	 */
	Metadata(std::istream& s);

	//! \brief Destructs the metadata object
	~Metadata();

	friend std::ostream& operator<<(std::ostream& os, const Metadata& md);

private:
	/*! \brief Retrieves the next byte from the metadata buffer
	 *  \throws MetaDataException on failure
	 */
	uint8_t getByte();

	/*! \brief Retrieves the next base 10 integer value
	 *  \param terminator Terminator charachter, ends the value
	 *  \param curch Current charachter
	 */
	uint64_t getInteger(uint8_t terminator, uint8_t curch);

	//! \brief List containing all metadata fields
	std::list<MetaField*> fields;

	/* \brief Attemps to parse a field from the stream
	 * \returns Field constructed, or NULL on end of list/dictionary/stream marker
	 */
	MetaField* handleField();

	//! \brief The input stream used
	std::istream& is;
};

#endif /* __METADATA_H__ */
