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
#include "typemapper.hpp"

class IStream;
class OStream;

class Mutex;


namespace serialization{
namespace bin{

BASIC_DECL(int16);
BASIC_DECL(uint16);
BASIC_DECL(int32);
BASIC_DECL(uint32);
BASIC_DECL(int64);
BASIC_DECL(uint64);

enum {
	MAXITSZ = sizeof(int64) + sizeof(int64),//!< Max sizeof(iterator) for serialized containers
	MINSTREAMBUFLEN = 128//if the free space for current buffer is less than this value
						//storring a stream will end up returning NOK
};
//===============================================================
class Base{
protected:
	struct FncData;
	typedef int (*FncTp)(Base &, FncData &);
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
	static int popEStack(Base &_rs, FncData &_rfd);
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
		TypeMapper::map<TM, Serializer>(_rfd.p, rs, _rfd.n, rs.tmpstr);
		return CONTINUE;
	}
	template <typename T>
	static int store(Base &_rs, FncData &_rfd){
		idbg("store generic non pointer");
		Serializer &rs(static_cast<Serializer&>(_rs));
		if(!rs.cpb) return OK;
		T& rt = *((T*)_rfd.p);
		rs.fstk.pop();
		rt & rs;
		return CONTINUE;
	}
	static int storeBinary(Base &_rs, FncData &_rfd);
	template <typename T>
	static int storeContainer(Base &_rs, FncData &_rfd){
		idbg("store generic non pointer container sizeof(iterator) = "<<sizeof(typename T::iterator));
		Serializer &rs(static_cast<Serializer&>(_rs));
		if(!rs.cpb) return OK;
		T * c = reinterpret_cast<T*>(_rfd.p);
		cassert(sizeof(typename T::iterator) <= sizeof(ExtData));
		rs.estk.push(ExtData());
		typename T::iterator *pit(new(rs.estk.top().buf) typename T::iterator(c->begin()));
		//typename T::const_iterator &pit = *reinterpret_cast<typename T::const_iterator*>(estk.top().buf);
		*pit = c->begin();
		rs.estk.push(ExtData(c->size()));
		_rfd.f = &Serializer::storeContainerContinue<T>;
		rs.fstk.push(FncData(&Base::popEStack, NULL));
		rs.fstk.push(FncData(&Serializer::template store<uint32>, &rs.estk.top().u32()));
		return CONTINUE;
	}
	
	template <typename T>
	static int storeContainerContinue(Base &_rs, FncData &_rfd){
		Serializer &rs(static_cast<Serializer&>(_rs));
		typename T::iterator &rit = *reinterpret_cast<typename T::iterator*>(rs.estk.top().buf);
		T * c = reinterpret_cast<T*>(_rfd.p);
		if(rs.cpb && rit != c->end()){
			rs.push(*rit);
			++rit;
			return CONTINUE;
		}else{
			//TODO:?!how
			//rit.T::~const_iterator();//only call the destructor
			rs.estk.pop();
			return OK;
		}
	}
public:
	template <class Map>
	Serializer(Map *p):ptypeidf(&storeTypeId<Map>), pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(ulong));
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
	template <typename T>
	Serializer& pushContainer(T &_t, const char *_name = NULL){
		fstk.push(FncData(&Serializer::template storeContainer<T>, (void*)&_t, _name));
		return *this;
	}
private:
	friend class TypeMapper;
	FncTp		ptypeidf;
	char		*pb;
	char		*cpb;
	char		*be;
	std::string	tmpstr;
};
//===============================================================
class Deserializer: public Base{
	template <typename T>
	static int parse(Base& _rd, FncData &_rfd){
		idbg("parse generic non pointer");
		Deserializer &rd(static_cast<Deserializer&>(_rd));
		if(!rd.cpb) return OK;
		rd.fstk.pop();
		*reinterpret_cast<T*>(_rfd.p) & rd;
		return CONTINUE;
	}
	template <class TM>
	static int parseTypeIdDone(Base& _rd, FncData &_rfd){
		Deserializer &rd(static_cast<Deserializer&>(_rd));
		uint32 *pu = reinterpret_cast<uint32*>(const_cast<char*>(rd.tmpstr.data()));
		idbg("pu = "<<*pu);
		void *p = _rfd.p;
		rd.fstk.pop();
		TypeMapper::map<TM, Deserializer, Serializer>(p, rd, rd.tmpstr);
		return CONTINUE;
	}
	template <class TM>
	static int parseTypeId(Base& _rd, FncData &_rfd){
		Deserializer &rd(static_cast<Deserializer&>(_rd));
		uint32 *pu = reinterpret_cast<uint32*>(const_cast<char*>(rd.tmpstr.data()));
		idbg("pu = "<<*pu);
		TypeMapper::parseTypeId<TM, Deserializer>(rd, rd.tmpstr);
		_rfd.f = &parseTypeIdDone<TM>;
		return CONTINUE;
	}
	static int parseBinary(Base &_rb, FncData &_rfd);
	static int parseBinaryString(Base &_rb, FncData &_rfd);
	template <typename T>
	static int parseContainer(Base &_rb, FncData &_rfd){
		idbg("parse generic non pointer container");
		Deserializer &rd(static_cast<Deserializer&>(_rb));
		if(!rd.cpb) return OK;
		_rfd.f = &Deserializer::parseContainerBegin<T>;
		rd.estk.push(ExtData(0));
		rd.fstk.push(FncData(&Deserializer::parse<uint32>, &rd.estk.top().u32()));
		return CONTINUE;
	}
	template <typename T>
	static int parseContainerBegin(Base &_rb, FncData &_rfd){
		Deserializer &rd(static_cast<Deserializer&>(_rb));
		if(!rd.cpb) return OK;
		uint32 ul = rd.estk.top().u32();
		rd.estk.pop();
		if(ul){
			_rfd.f = &Deserializer::parseContainerContinue<T>;
			_rfd.s = ul;
			return CONTINUE;
		}
		return OK;
	}
	template <typename T>
	static int parseContainerContinue(Base &_rb, FncData &_rfd){
		Deserializer &rd(static_cast<Deserializer&>(_rb));
		if(rd.cpb && _rfd.s){
			T *c = reinterpret_cast<T*>(_rfd.p);
			c->push_back(typename T::value_type());
			rd.push(c->back());
			--_rfd.s;
			return CONTINUE;	
		}
		return OK;
	}
public:
	template <class Map>
	Deserializer(Map *):ptypeidf(&parseTypeId<Map>), pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(ulong));
	}
	~Deserializer();
	void clear();
	bool empty()const {return fstk.empty();}
	int run(const char *_pb, unsigned _bl);
	
	template <typename T>
	Deserializer& push(T &_t, const char *_name = NULL){
		idbg("push nonptr "<<_name);
		fstk.push(FncData(&Deserializer::parse<T>, (void*)&_t, _name));
		return *this;
	}
	
	template <typename T>
	Deserializer& push(T* &_t, const char *_name = NULL){
		idbg("push ptr "<<_name);
		fstk.push(FncData(ptypeidf, &_t, _name));
		return *this;
	}
	//! Schedules a std (style) container for deserialization
	template <typename T>
	Deserializer& pushContainer(T &_t, const char *_name = NULL){
		fstk.push(FncData(&Deserializer::template parseContainer<T>, (void*)&_t, _name));
		return *this;
	}
private:
	FncTp		ptypeidf;
	const char	*pb;
	const char	*cpb;
	const char	*be;
	std::string	tmpstr;
};

//! Nonintrusive string serialization/deserialization specification
template <class S>
S& operator&(std::string &_t, S &_s){
	return _s.push(_t, "string");
}


}//namespace bin
}//namespace serialization


#endif
