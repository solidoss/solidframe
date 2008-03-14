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

#include "basetypemap.hpp"

namespace serialization{

class TypeMapper{
	typedef BaseTypeMap::FncTp	FncTp;
public:
	static TypeMapper& the(){
		static TypeMapper tm;
		return tm;
	}
	template <class Map>
	void registerMap(Map *_pm){
		the().doRegisterMap(mapId<Map>, _pm);
	}
	template <class Ser>
	void registerSerializer(){
		the().serializerCount(serializerId<Ser>());
	}
	template <class T, class Ser, class Des>
	static void map(){
		the().doMap(&TypeMapper::doMapCallback<T, Ser, Des>, serializerId<Ser>());
	}
	template <class TM, class Ser>
	static void map(void *_p, Ser &_rs, const char *_name){
		TM &tm(static_cast<TM&>(the().getMap(mapId<TM>())));
		FncTp pf = tm.storeTypeId(_rs, _name);
		if(pf){
			(*pf)(_p, &_rs, NULL);
		}
	}
	template <class TM, class Des>
	static void parseTypeId(Des &_rd, std::string &_str, ulong &_rul){
		TM &tm(static_cast<TM&>(the().getMap(mapId<TM>())));
		tm.parseTypeIdPrepare(_rd, _str, _rul);
	}
	template <class TM, class Des>
	static void map(void *_p, Des &_rd, const std::string &_rs, const ulong &_ul){
		TM &tm(static_cast<TM&>(the().getMap(mapId<TM>())));
		FncTp pf = tm.parseTypeIdDone(_rs, _ul);
		if(pf){
			(*pf)(_p, NULL, &_rd);
		}
	}
private:
	template <class T>
	static unsigned mapId(){
		static unsigned const d(newMapId());
		return d;
	}
	template <class T>
	static unsigned serializerId(){
		static unsigned const d(newSerializerId());
		return d;
	}
	TypeMapper();
	static unsigned newMapId();
	static unsigned newSerializerId();
	void doRegisterMap(unsigned _id, BaseTypeMap *_pbm);
	void doMap(FncTp _pf, unsigned _pos);
	void serializerCount(unsigned _cnt);
	BaseMap &getMap(unsigned _id);
	template <class T, class Ser, class Des>
	void doMapCallback(void *_p, void *_ps, void *_pd){
		if(_ps){
			Ser &rs(*reinterpret_cast<Ser*>(_ps));
			T   &rt(*reinterpret_cast<T*>(_p));
			t & rs;
		}else{
			Des &rd(*reinterpret_cast<Des*>(_pd));
			T*  &rpt(*reinterpret_cast<T**>(_p));
			rpt = new T;
			&rpt & rd;
		}
	}
private:
	struct Data;
	Data	&d;
};

}

#endif
