#include <list>
#include <stdint.h>
#include <iostream>
#include <stdint.h>
#include <string>
#include "metafield.h"

#ifndef __TORTILLA_METADATA_H__
#define __TORTILLA_METADATA_H__

/*! \brief Contains torrent file metadata
 */
class Metadata {
public:
	/*! \brief Constructs a new metadata object
	 *  \param s Stream to construct the metadata from
	 */
	Metadata(std::istream& s);

	//! \brief Constructs a new empty metadata object
	Metadata();

	//! \brief Constructs a metadata object from a given dictionary
	Metadata(MetaDictionary& md);

	//! \brief Destructs the metadata object
	~Metadata();

	//! Used for streaming the metadata
	friend std::ostream& operator<<(std::ostream& os, const Metadata& md);

	//! \brief Retrieve the dictionary
	inline MetaDictionary* getDictionary() { return dictionary; }

	//! \brief Retrieve the 'announce' URL
	std::string getAnnounceURL();

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

	//! \brief Our metainfofile dictionary
	MetaDictionary* dictionary;

	/* \brief Attemps to parse a field from the stream
	 * \returns Field constructed, or NULL on end of list/dictionary/stream marker
	 */
	MetaField* handleField();

	//! \brief The input stream used
	std::istream* is;
};

#endif /* __TORTILLA_METADATA_H__ */
