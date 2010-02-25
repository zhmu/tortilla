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

MetaList::MetaList(MetaList& ml)
{
	std::list<MetaField*> srcList = ml.getList();
	for (std::list<MetaField*>::const_iterator it = srcList.begin();
	     it != srcList.end(); it++) {
		list.push_back(MetaField::clone(*it));
	}
}

std::ostream& operator<<(std::ostream& os, const StringFieldMap& sfm)
{
	MetaString ms(sfm.key);
	os << ms;
	os << *sfm.value;
	return os;
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

MetaDictionary::~MetaDictionary()
{
	std::list<StringFieldMap*>::iterator it = dictionary.begin();
	while (it != dictionary.end()) {
		StringFieldMap* sfm = *it;
		dictionary.erase(it);
		delete sfm->getValue();
		delete sfm;
		it = dictionary.begin();
	}
}

MetaDictionary::MetaDictionary(MetaDictionary& src)
{
	std::list<StringFieldMap*> srcDictionary = src.getDictionary();

	for (std::list<StringFieldMap*>::const_iterator it = srcDictionary.begin();
	     it != srcDictionary.end(); it++) {
		StringFieldMap* sfm = *it;
		assign(sfm->getKey(), MetaField::clone(sfm->getValue()));
	}
}

MetaField*
MetaField::clone(MetaField* src)
{
	/*
	 * I wish I could just use MetaField::MetaField() as copy constructor; but it
	 * has a pure virtual, so that won't work :-(
	 */
	MetaString* ms = dynamic_cast<MetaString*>(src);
	if (ms != NULL)
		return new MetaString(*ms);
	MetaInteger* mi = dynamic_cast<MetaInteger*>(src);
	if (mi != NULL)
		return new MetaInteger(*mi);
	MetaList* ml = dynamic_cast<MetaList*>(src);
	if (ml != NULL)
		return new MetaList(*ml);
	MetaDictionary* md = dynamic_cast<MetaDictionary*>(src);
	if (md != NULL)
		return new MetaDictionary(*md);
	return NULL;
}

/* vim:set ts=2 sw=2: */
