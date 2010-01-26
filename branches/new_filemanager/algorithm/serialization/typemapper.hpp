/* Declarations file typemapper.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef TYPE_MAPPER_HPP
#define TYPE_MAPPER_HPP

#include <string>
#include <typeinfo>
#include "utility/common.hpp"
#include "basetypemap.hpp"

#define BASIC_DECL(tp) \
template <class S>\
S& operator&(tp &_t, S &_s){\
	return _s.push(_t, "basic");\
}


namespace serialization{

BASIC_DECL(int16);
BASIC_DECL(uint16);
BASIC_DECL(int32);
BASIC_DECL(uint32);
BASIC_DECL(int64);
BASIC_DECL(uint64);

//! Maps types to be used on serialization
/*!
	Conde sample:
	<code>
	typedef serialization::TypeMapper					TypeMapper;<br>
	//typedef serialization::NameTypeMap				NameTypeMap;<br>
	typedef serialization::IdTypeMap					IdTypeMap;<br>
	typedef serialization::bin::Serializer				BinSerializer;<br>
	typedef serialization::bin::Deserializer			BinDeserializer;<br>
	<br>
	TypeMapper::registerMap<IdTypeMap>(new IdTypeMap);<br>
	<br>
	TypeMapper::registerSerializer<BinSerializer>();<br>
	<br>
	TypeMapper::map<String, BinSerializer, BinDeserializer>();<br>
	TypeMapper::map<UnsignedInteger, BinSerializer, BinDeserializer>();<br>
	TypeMapper::map<IntegerVector, BinSerializer, BinDeserializer>();<br>
	</code>
	<br>
	In the above code, we register a type map using ids for types,
	then we register the serializer (the binary one) an then we map the 
	actual types: a String, an UnsignedInterger and an IntegerVector. <br>
	The mapping must be done given the serializer/and deserializer.
	So if we would have lets say XmlSerializer and XmlDeserializer, we would
	have to do:
	<br>
	<code>
	//...<br>
	TypeMapper::registerSerializer<XmlSerializer>();<br>
	//...<br>
	TypeMapper::map<String, XmlSerializer, XmlDeserializer>();<br>
	//...<br>
	</code>
*/
class TypeMapper{
	typedef BaseTypeMap::FncTp	FncTp;
	template <class T>
	static unsigned mapId(){
		//TODO: staticproblem
		static unsigned const d(newMapId());
		return d;
	}
	template <class T>
	static unsigned serializerId(){
		//TODO: staticproblem
		static unsigned const d(newSerializerId());
		return d;
	}
public:
	static TypeMapper& the(){
		//TODO: staticproblem
		static TypeMapper tm;
		return tm;
	}
	~TypeMapper();
	template <class T>
	static const char* typeName(T *_p){
		return typeid(*_p).name();
	}
	//! Register a type map
	template <class Map>
	static void registerMap(Map *_pm){
		the().doRegisterMap(mapId<Map>(), _pm);
	}
	//! Register a serializer
	template <class Ser>
	static void registerSerializer(){
		the().serializerCount(serializerId<Ser>());
	}
	//! Register a type for a specific serializer/deserializer pair
	template <class T, class Ser, class Des>
	static void map(){
		the().doMap(&TypeMapper::doMapCallback<T, Ser, Des>, serializerId<Ser>(), typeid(T).name());
	}
	template <class TM, class Ser>
	static void map(void *_p, Ser &_rs, const char *_name, std::string &_rstr){
		TM &tm(static_cast<TM&>(the().getMap(mapId<TM>())));
		the().lock();
		tm.storeTypeId(_rs, _name, _rstr, serializerId<Ser>(), _p);
		the().unlock();
	}
	template <class TM, class Des>
	static void parseTypeId(Des &_rd, std::string &_rstr){
		TM &tm(static_cast<TM&>(the().getMap(mapId<TM>())));
		tm.parseTypeIdPrepare(_rd, _rstr);
	}
	template <class TM, class Des, class Ser>
	static void map(void *_p, Des &_rd, const std::string &_rstr){
		TM &tm(static_cast<TM&>(the().getMap(mapId<TM>())));
		the().lock();
		FncTp pf = tm.parseTypeIdDone(_rstr, serializerId<Ser>());
		the().unlock();
		if(pf){
			(*pf)(_p, NULL, &_rd);
		}
	}
private:
	TypeMapper();
	static unsigned newMapId();
	static unsigned newSerializerId();
	void doRegisterMap(unsigned _id, BaseTypeMap *_pbm);
	void doMap(FncTp _pf, unsigned _pos, const char *_name);
	void serializerCount(unsigned _cnt);
	BaseTypeMap &getMap(unsigned _id);
	template <class T, class Ser, class Des>
	static void doMapCallback(void *_p, void *_ps, void *_pd){
		if(_ps){
			Ser &rs(*reinterpret_cast<Ser*>(_ps));
			T   &rt(*reinterpret_cast<T*>(_p));
			rt & rs;
		}else{
			Des &rd(*reinterpret_cast<Des*>(_pd));
			T*  &rpt(*reinterpret_cast<T**>(_p));
			rpt = new T;
			*rpt & rd;
		}
	}
	void lock();
	void unlock();
private:
	struct Data;
	Data	&d;
};



}

#endif
