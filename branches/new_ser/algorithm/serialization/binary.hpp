/* Declarations file binary.hpp
	
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

#ifndef SERIALIZATION_BINARY_HPP
#define SERIALIZATION_BINARY_HPP

#include <typeinfo>
#include <string>

#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "utility/common.hpp"
#include "utility/stack.hpp"

#define NUMBER_DECL(tp) \
template <class S>\
S& operator&(tp &_t, S &_s){\
	return _s.pushNumber(&_t, sizeof(tp), "binary");\
}

class IStream;
class OStream;

class Mutex;

namespace serialization{
namespace bin{

enum {
	MAXITSZ = sizeof(int64) + sizeof(int64),//!< Max sizeof(iterator) for serialized containers
	MINSTREAMBUFLEN = 128//if the free space for current buffer is less than this value
						//storring a stream will end up returning NOK
};
//===============================================================
class Base{
protected:
	struct FncData;
	typedef int (Base::*FncTp)(Base &, FncData &);
	//! Data associated to a callback
	struct FncData{
		FncData(
			FncTp _f,
			void *_p,
			const char *_n = NULL,
			ulong _s = -1
		):f(_f),p(_p),n(_n),s(_s){}
		
		FncTp		f;	//!< Pointer to function
		void		*p;	//!< Pointer to data
		const char 	*n;	//!< Some name - of the item serialized
		ulong		s;	//!< Some size
	};
	
	struct ExtData{
		char 	buf[MAXITSZ];
		
		const uint32& u32()const{return *reinterpret_cast<const uint32*>(buf);}
		uint32& u32(){return *reinterpret_cast<uint32*>(buf);}
		const uint64& u64()const{return *reinterpret_cast<const uint64*>(buf);}
		uint64& u64(){return *reinterpret_cast<uint64*>(buf);}
		
		ExtData(){}
		ExtData(uint32 _u32){u32() = _u32;}
	};
protected:
	//! Replace the top callback from the stack
	void replace(const FncData &_rfd);
protected:
	typedef Stack<FncData>	FncDataStackTp;
	typedef Stack<ExtData>	ExtDataStackTp;
	ulong				ul;
	FncDataStackTp		fstk;
	ExtDataStackTp		estk;
};
//===============================================================
class Serializer: public Base{
	template <class TM>
	static int storeTypeId(Base &_rs, FncData &_rfd){
		Serializer &rs(static_cast<Serializer&>(_rs));
		FncData fd(_rfd);
		rs.fstk.pop();
		//TypeMapper::getMap<TM>(). template storeTypeId<Serializer>(rs, fd.p);
		//schedules the store of the typeid
		//and the data
		/*TODO:
			the integer id must be stored somewhere
			for serialization - think of a way to do that!!
		*/
		TypeMapper::map<TM, Serializer>(_rfd.p, &rs, _rfd.n, tmpstr);
		return CONTINUE;
	}
	template <typename T>
	int store(Base &_rs, FncData &_rfd){
		idbg("store generic non pointer");
		Serializer &rs(static_cast<Serializer&>(_rs));
		if(!rs.cpb) return OK;
		T& rt = *((T*)_rfd.p);
		rs.fstk.pop();
		rt & rs;
		return CONTINUE;
	}
public:
	template <class Map>
	Serializer():ptypeidf(&storeTypeId), pb(NULL), cpb(NULL), be(NULL){
	}
	~Serializer();
	void clear();
	bool empty()const {return fstk.empty();}
	int run(char *_pb, unsigned _bl);
	
	//! Schedule a non pointer object for serialization
	/*!
		The object is only scheduled for serialization so it must remain in memory
		up until the serialization will end.
		
		The given name is meaningless for binary serialization, it will be usefull for
		text oriented serialization, and I want a common interface for push, so one
		can write a single template function for serializing an object.
	*/
	template <typename T>
	Serializer& push(T &_t, const char *_name = NULL){
		idbg("push nonptr "<<_name);
		FncTp	tpf;
		fstk.push(FncData(&Serializer::template store<T>, (void*)&_t, _name));
		return *this;
	}
	//! Schedule a pointer object for serialization
	/*!
		The object is only scheduled for serialization so it must remain in memory
		up until the serialization will end.
		
		The given name is meaningless for binary serialization, it will be usefull for
		text oriented serialization, and I want a common interface for push, so one
		can write a single template function for serializing an object.
	*/
	template <typename T>
	Serializer& push(T* _t, const char *_name = NULL){
		idbg("push ptr "<<_name);
		fstk.push(FncData(ptypeidf, _t, _t ? TypeMapper::typeName(_t) : NULL));
		return *this;
	}
private:
	friend class TypeMapper;
	FncTp	ptypeidf;
	char	*pb;
	char	*cpb;
	char	*be;
};
//===============================================================
class Deserializer: public Base{
	template <typename T>
	int parse(Base& _rd, FncData &_rfd){
		idbg("parse generic non pointer");
		if(!cpb.ps) return OK;
		_rd.fstk.pop();
		Deserializer &rd(static_cast<Deserializer&>(_rd));
		*reinterpret_cast<T*>(_rfd.p) & rd;
		return CONTINUE;
	}
	template <class TM>
	static int parseTypeIdDone(Base& _rd, FncData &_rfd){
		Deserializer &rd(static_cast<Deserializer&>(_rd));
		void *p = _rfd.p;
		rd.fstk.pop();
		TypeMapper::map<TM, Deserializer>(p, &rd, &tmpstr, _rfd.s);
		return CONTINUE;
	}
	template <class TM>
	static int parseTypeId(FncData &_rfd){
		Deserializer &rd(static_cast<Deserializer&>(_rd));
		//TypeMapper::getMap<TM>(). template parseTypeId<Deserializer>(rd, tmpstr, _rfd.p);
		TypeMapper::parseTypeId<TM, Deserializer>(rd, tmpstr, _rfd.s);
		_rfd.f = &parseTypeIdDone<TM>;
		return CONTINUE;
	}
public:
	template <class Map>
	Deserializer():ptypeidf(&parseTypeId), pb(NULL), cpb(NULL), be(NULL){
	}
	~Deserializer();
	void clear();
	bool empty()const {return fstk.empty();}
	int run(const char *_pb, unsigned _bl);
	
	template <typename T>
	Deserializer& push(T &_t, const char *_name = NULL){
		idbg("push nonptr "<<_name);
		FncTp tpf;
		fstk.push(FncData(&Deserializer::parse<T>, (void*)&_t, _name));
		return *this;
	}
	
	template <typename T>
	Deserializer& push(T* &_t, const char *_name = NULL){
		idbg("push ptr "<<_name);
		fstk.push(FncData(ptypeidf, &_t, _name));
		return *this;
	}
private:
	FncTp		ptypeidf;
	string		tmpstr;
	const char	*pb;
	const char	*cpb;
	const char	*be;
};

}//namespace bin
}//namespace serialization


#endif
