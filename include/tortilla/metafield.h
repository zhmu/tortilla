#include <ostream>
#include <list>
#include <map>
#include <string>

#ifndef __METAFIELD_H__
#define __METAFIELD_H__

class MetaField {
public:
	friend std::ostream& operator<<(std::ostream& os, const MetaField& mf);
	friend std::istream& operator>>(std::istream& is, const MetaField& mf);

	inline virtual ~MetaField() { }

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
	inline const std::string getString() { return string; }

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

	inline uint64_t getInteger() { return integer; }

protected:
	void stream(std::ostream& o) const;

private:
	uint64_t integer;
};

class MetaList : public MetaField {
public:
	inline void addItem(MetaField* f) {
		list.push_back(f);
	}

	inline std::list<MetaField*>& getList() { return list; }

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

	std::string getKey() { return key; }
	MetaField* getValue() { return value; }

	friend std::ostream& operator<<(std::ostream& os, const StringFieldMap& sfm);

private:
	//! \brief Name part of the map
	std::string key;

	//! \brief Value part of the map
	MetaField* value;
};

class MetaDictionary : public MetaField {
public:
	virtual ~MetaDictionary();

	inline void assign(std::string str, MetaField* f) {
		StringFieldMap* sfm = new StringFieldMap(str, f);
		dictionary.push_back(sfm);
	}

	MetaField* operator[](std::string key);

protected:
	void stream(std::ostream& o) const;

private:
	std::list<StringFieldMap*> dictionary;
};

#endif /* __METAFIELD_H__ */
