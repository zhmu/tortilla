#include <ostream>
#include <string>
#include "exceptions.h"
#include "metadata.h"
#include "metafield.h"

using namespace std;

Tortilla::MetaField*
Tortilla::Metadata::handleField()
{
	uint8_t b = getByte();
	if (is->eof())
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
				throw MetadataException("dictionary entry isn't followed by a string");

			/* get the value part, this must not be the end specifier */
			MetaField* value = handleField();
			if (value == NULL)
				throw MetadataException("dictionary entry doesn't have a value");

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
		throw MetadataException("unsupported field type");
}

Tortilla::Metadata::Metadata(std::istream& s)
{
	is = &s;

	dictionary = dynamic_cast<MetaDictionary*>(handleField());
	if (dictionary == NULL)
		throw MetadataException("metadata content isn't a dictionary");
}

Tortilla::Metadata::Metadata()
{
	is = NULL;
	dictionary = new MetaDictionary();
}

Tortilla::Metadata::Metadata(MetaDictionary& md)
{
	is = NULL;
	dictionary = new MetaDictionary(md);
}

Tortilla::Metadata::~Metadata()
{
	delete dictionary;
}

uint8_t
Tortilla::Metadata::getByte()
{
	uint8_t c;
	is->read((char*)&c, 1);
	return c;
}

uint64_t
Tortilla::Metadata::getInteger(uint8_t terminator, uint8_t curch)
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
		throw MetadataException("integer type followed by non-terminator digit");
	}
}

ostream&
Tortilla::operator<<(ostream& os, const Metadata& md)
{
	os << *md.dictionary;
	return os;
}

/* vim:set ts=2 sw=2: */
