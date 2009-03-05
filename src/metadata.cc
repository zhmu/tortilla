#include <ostream>
#include <string>
#include "metadata.h"
#include "metafield.h"

using namespace std;

MetaField*
Metadata::handleField()
{
	uint8_t b = getByte();
	if (is.eof())
		return NULL;

	if (b >= '0' && b <= '9') {
		/* string: <length>:<data> */
		uint64_t len = getInteger(':', b);
		string s;
		while (len--) {
			b = getByte();
			s += b;
		}
		return new MetaString(s);
	} else if (b == 'i') {
		/* integer: i<number>e */
		return new MetaInteger(getInteger('e', b));
	} else if (b == 'l') {
		/* list: l<elements>e */
		MetaList* list = new MetaList();
		while (MetaField* field = handleField()) {
			list->addItem(field);
		}
		return list;
	} else if (b == 'd') {
		/*
		 * dictionary: d<entries>e
 		 * each entry is <string><value>, ie. <length>:<data><value>
 		 */
		MetaDictionary* dict = new MetaDictionary();
		while (1) {
			/* fetch the key part of the dictionary; this must be a string */
			MetaField* key = handleField();
			if (key == NULL)
				/* end of dictionary found */
				break;
			MetaString* string = dynamic_cast<MetaString*>(key);
			if (string == NULL)
				throw MetaDataException(METADATA_CODE_CORRUPT); 

			/* get the value part, this must not be the end specifier */
			MetaField* value = handleField();
			if (value == NULL)
				throw MetaDataException(METADATA_CODE_CORRUPT); 

			/* assign the value */
			dict->assign(string->getString(), value);
			delete key;
		}
		return dict;
	} else if (b == 'e') {
		/* end of list/dictionary marker */
		return NULL;
	} else
		/* ? */
		throw MetaDataException(METADATA_CODE_CORRUPT); 
}

Metadata::Metadata(std::istream& s)
	 : is(s)
{
	while (1) {
		MetaField* f = handleField();
		if (f == NULL)
			break;
		fields.push_back(f);
	}

}

Metadata::~Metadata()
{
}

uint8_t
Metadata::getByte()
{
	uint8_t c;
	is.read((char*)&c, 1);
	return c;
}

uint64_t
Metadata::getInteger(uint8_t terminator, uint8_t curch)
{
	uint64_t v = 0;

	if (curch >= '0' && curch <= '9')
		v = (curch - '0');

	while (1) {
		uint8_t b = getByte();
		if (b >= '0' && b <= '9') {
			v *= 10;
			v += (b - '0');
			continue;
		}
		if (b == terminator)
			return v;
		throw MetaDataException(METADATA_CODE_CORRUPT); 
	}
}

ostream&
operator<<(ostream& os, const Metadata& md)
{

	for (list<MetaField*>::const_iterator it = md.fields.begin();
	     it != md.fields.end(); it++) {
		os << **it;
	}
	return os;
}

/* vim:set ts=2 sw=2: */
