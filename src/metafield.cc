#include <iostream>
#include <sstream>
#include <ostream>
#include <list>
#include <string>
#include "metafield.h"

using namespace std;

ostream& operator<<(ostream& os, const MetaField& mf)
{
	mf.stream(os);
	return os;
}

void MetaString::stream(ostream& os) const
{
	os << string.size(); os << ":"; os << string;
}

void MetaInteger::stream(ostream& os) const
{
	os << "i"; os << integer; os << "e";
}


void MetaList::stream(ostream& os) const
{
	os << "l";
	for (std::list<MetaField*>::const_iterator it = list.begin();
	     it != list.end(); it++) {
		os << **it;
	}
	os << "e";
}

std::ostream& operator<<(std::ostream& os, const StringFieldMap& sfm)
{
	MetaString ms(sfm.key);
	os << ms;
	os << *sfm.value;
}

void MetaDictionary::stream(ostream& os) const
{
	os << "d";
	for (std::list<StringFieldMap*>::const_iterator it = dictionary.begin();
	     it != dictionary.end(); it++) {
		os << **it;
	}
	os << "e";
}

MetaField* MetaDictionary::operator[](std::string key)
{
	for (std::list<StringFieldMap*>::const_iterator it = dictionary.begin();
	     it != dictionary.end(); it++) {
		StringFieldMap* sfm = *it;
		if (sfm->getKey() == key)
			return sfm->getValue();
	}
	return NULL;
}

void MetaDictionary::dump()
{
	for (std::list<StringFieldMap*>::const_iterator it = dictionary.begin();
	     it != dictionary.end(); it++) {
		StringFieldMap* sfm = *it;
		printf("%s\n", sfm->getKey().c_str());
	}
}

/* vim:set ts=2 sw=2: */
