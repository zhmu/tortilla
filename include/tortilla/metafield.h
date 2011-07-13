#include <ostream>
#include <list>
#include <map>
#include <stdint.h>
#include <string>
#include <stdint.h>

#ifndef __TORTILLA_METAFIELD_H__
#define __TORTILLA_METAFIELD_H__

namespace Tortilla {

class MetaField {
public:
	friend std::ostream& operator<<(std::ostream& os, const MetaField& mf);
	friend std::istream& operator>>(std::istream& is, const MetaField& mf);

	MetaField() { };
	inline virtual ~MetaField() { }

	static MetaField* clone(const MetaField* src);

protected:
	/*! \brief Stream field to an output stream
	 *
	 *  This is needed due to inheritence of MetaField.
	 */
	virtual void stream(std::ostream& o) const = 0;
};

class MetaString : public MetaField {
public:
	inline MetaString(std::string s) { string = s; }
	inline MetaString(const MetaString& ms) { string = ms.getString(); }
	inline const std::string& getString() const { return string; }

protected:
	void stream(std::ostream& o) const;

private:
	std::string string;
};

class MetaInteger : public MetaField {
public:
	inline MetaInteger (uint64_t i) {
		integer = i;
	}
	inline MetaInteger(const MetaInteger& mi) {
		integer = mi.getInteger();
	}

	inline uint64_t getInteger() const { return integer; }

protected:
	void stream(std::ostream& o) const;

private:
	uint64_t integer;
};

class MetaList : public MetaField {
public:
	MetaList() { };
	MetaList(const MetaList& ml);

	inline void addItem(MetaField* f) {
		list.push_back(f);
	}

	inline const std::list<MetaField*>& getList() const { return list; }

protected:
	void stream(std::ostream& o) const;

private:
	std::list<MetaField*> list;
};

class StringFieldMap {
public:
	inline StringFieldMap(std::string k, MetaField* v) {
		key = k; value = v;
	}

	const std::string& getKey() const { return key; }
	const MetaField* getValue() const { return value; }

	friend std::ostream& operator<<(std::ostream& os, const StringFieldMap& sfm);

private:
	//! \brief Name part of the map
	std::string key;

	//! \brief Value part of the map
	MetaField* value;
};

class MetaDictionary : public MetaField {
public:
	MetaDictionary() { };
	MetaDictionary(const MetaDictionary& mi);
	virtual ~MetaDictionary();

	inline void assign(const std::string& str, MetaField* f) {
		StringFieldMap* sfm = new StringFieldMap(str, f);
		dictionary.push_back(sfm);
	}

	const MetaField* operator[](std::string key) const;

protected:
	void stream(std::ostream& o) const;
	const std::list<StringFieldMap*>& getDictionary() const { return dictionary; }

private:
	std::list<StringFieldMap*> dictionary;
};

}

#endif /* __TORTILLA_METAFIELD_H__ */
