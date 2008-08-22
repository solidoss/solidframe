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

namespace serialization{
namespace bin{

BASIC_DECL(int16);
BASIC_DECL(uint16);
BASIC_DECL(int32);
BASIC_DECL(uint32);
BASIC_DECL(int64);
BASIC_DECL(uint64);
BASIC_DECL(ulong);

//! Nonintrusive string serialization/deserialization specification
template <class S>
S& operator&(std::string &_t, S &_s){
	return _s.push(_t, "string");
}

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
		const int32& i32()const{return *reinterpret_cast<const int32*>(buf);}
		int32& i32(){return *reinterpret_cast<int32*>(buf);}
		
		ExtData(){}
		ExtData(uint32 _u32){u32() = _u32;}
		ExtData(int32 _i32){i32() = _i32;}
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
		if(c){
			cassert(sizeof(typename T::iterator) <= sizeof(ExtData));
			rs.estk.push(ExtData());
			typename T::iterator *pit(new(rs.estk.top().buf) typename T::iterator(c->begin()));
			//typename T::const_iterator &pit = *reinterpret_cast<typename T::const_iterator*>(estk.top().buf);
			*pit = c->begin();
			rs.estk.push(ExtData((int32)c->size()));
			_rfd.f = &Serializer::storeContainerContinue<T>;
			rs.fstk.push(FncData(&Base::popEStack, NULL));
			rs.fstk.push(FncData(&Serializer::template store<int32>, &rs.estk.top().i32()));
		}else{
			rs.estk.push(ExtData((int32)(-1)));
			rs.fstk.pop();
			rs.fstk.push(FncData(&Serializer::template store<int32>, &rs.estk.top().i32()));
		}
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
	template <typename T>
	static int storeStreamBegin(Base &_rs, FncData &_rfd){
		idbg("store stream begin");
		Serializer &rs(static_cast<Serializer&>(_rs));
		if(!rs.cpb) return OK;
		T *pt = reinterpret_cast<T*>(_rfd.p);
		std::pair<IStream*, int64> sp(NULL, -1);
		int cpidx = _rfd.s;
		++_rfd.s;
		switch(pt->createSerializationStream(sp, cpidx)){
			case BAD: sp.second = -1;break;//send -1
			case NOK: return OK;//no more streams
			case OK:
				cassert(sp.first);
				cassert(sp.second >= 0);
				break;
			default:
				cassert(false);
		}
		rs.estk.push(ExtData());
		std::pair<IStream*, int64> &rsp(*reinterpret_cast<std::pair<IStream*, int64>*>(rs.estk.top().buf));
		rsp = sp;
		//_rfd.s is the id of the stream
		rs.fstk.push(FncData(&Serializer::storeStreamDone<T>, _rfd.p, _rfd.n, _rfd.s - 1));
		if(sp.first){
			rs.fstk.push(FncData(&Serializer::storeStream, NULL));
		}
		rs.fstk.push(FncData(&Serializer::storeBinary, &rsp.second, _rfd.n, sizeof(int64)));
		return CONTINUE;
	}
	template <typename T>
	static int storeStreamDone(Base &_rs, FncData &_rfd){
		idbg("store stream done");
		Serializer &rs(static_cast<Serializer&>(_rs));
		std::pair<IStream*, int64> &rsp(*reinterpret_cast<std::pair<IStream*, int64>*>(rs.estk.top().buf));
		T *pt = reinterpret_cast<T*>(_rfd.p);
		pt->destroySerializationStream(rsp, _rfd.s);
		rs.estk.pop();
		return OK;
	}
	//! Internal callback for storing a stream
	static int storeStream(Base &_rs, FncData &_rfd);
public:
	enum {IsSerializer = true};
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
		//idbg("push nonptr "<<_name);
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
		//idbg("push ptr "<<_name);
		fstk.push(FncData(ptypeidf, _t, _t ? TypeMapper::typeName(_t) : NULL));
		return *this;
	}
	//! Schedules a stl style container for serialization
	template <typename T>
	Serializer& pushContainer(T &_t, const char *_name = NULL){
		fstk.push(FncData(&Serializer::template storeContainer<T>, (void*)&_t, _name));
		return *this;
	}
	//! Schedules a pointer to a stl style container for serialization
	template <typename T>
	Serializer& pushContainer(T *_t, const char *_name = NULL){
		fstk.push(FncData(&Serializer::template storeContainer<T>, (void*)_t, _name));
		return *this;
	}
	//! Schedules the serialization of an object containing streams.
	/*!
		Unfortunately this must be intrussive, that is the given streammer object must
		export a required interface.
		Please see the test/algorithm/serialization/fork.cpp example or 
		test::alpha::FetchSlaveCommand.
	*/
	template <typename T>
	Serializer& pushStreammer(T *_p, const char *_name = NULL){
		//idbg("push stream "<<_name);
		fstk.push(FncData(&Serializer::template storeStreamBegin<T>, _p, _name, 0));
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
		void *p = _rfd.p;
		rd.fstk.pop();
		TypeMapper::map<TM, Deserializer, Serializer>(p, rd, rd.tmpstr);
		return CONTINUE;
	}
	template <class TM>
	static int parseTypeId(Base& _rd, FncData &_rfd){
		Deserializer &rd(static_cast<Deserializer&>(_rd));
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
		rd.fstk.push(FncData(&Deserializer::parse<int32>, &rd.estk.top().i32()));
		return CONTINUE;
	}
	template <typename T>
	static int parseContainerBegin(Base &_rb, FncData &_rfd){
		Deserializer &rd(static_cast<Deserializer&>(_rb));
		if(!rd.cpb) return OK;
		int32 i = rd.estk.top().i32();
		idbg("i = "<<i);
		rd.estk.pop();
		if(i < 0){
			T **c = reinterpret_cast<T**>(_rfd.p);
			cassert(!*c);
			//*c = NULL;
			return OK;
		}else if(!_rfd.s){
			T **c = reinterpret_cast<T**>(_rfd.p);
			idbg("pass");
			*c = new T;
			_rfd.p = *c;
		}
		if(i){
			_rfd.f = &Deserializer::parseContainerContinue<T>;
			_rfd.s = (uint32)i;
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
	template <typename T>
	static int parseStreamBegin(Base &_rb,FncData &_rfd){
		idbg("parse stream begin");
		Deserializer &rd(static_cast<Deserializer&>(_rb));
		if(!rd.cpb) return OK;
		
		int cpidx = _rfd.s;
		++_rfd.s;
		std::pair<OStream*, int64> sp(NULL, -1);
		T	*pt = reinterpret_cast<T*>(_rfd.p);
		//TODO: createDeserializationStream should be given the size of the stream
		switch(pt->createDeserializationStream(sp, cpidx)){
			case BAD:	
			case OK: 	break;
			case NOK:	return OK;
		}
		rd.estk.push(ExtData());
		std::pair<OStream*, int64> &rsp(*reinterpret_cast<std::pair<OStream*, int64>*>(rd.estk.top().buf));
		rsp = sp;
		//_rfd.s is the id of the stream
		rd.fstk.push(FncData(&Deserializer::template parseStreamDone<T>, _rfd.p, _rfd.n, _rfd.s - 1));
		if(sp.first)
			rd.fstk.push(FncData(&Deserializer::parseStream, NULL));
		else
			rd.fstk.push(FncData(&Deserializer::parseDummyStream, NULL));
		//TODO: move this line - so it is called before parseStreamBegin
		rd.fstk.push(FncData(&Deserializer::parseBinary, &rsp.second, _rfd.n, sizeof(int64)));
		return CONTINUE;
	}
	template <typename T>
	static int parseStreamDone(Base &_rb,FncData &_rfd){
		idbg("parse stream done");
		Deserializer &rd(static_cast<Deserializer&>(_rb));
		std::pair<OStream*, int64> &rsp(*reinterpret_cast<std::pair<OStream*, int64>*>(rd.estk.top().buf));
		T	*pt = reinterpret_cast<T*>(_rfd.p);
		pt->destroyDeserializationStream(rsp, _rfd.s);
		rd.estk.pop();
		return OK;
	}
	//! Internal callback for parsing a stream
	static int parseStream(Base &_rb, FncData &_rfd);
	//! Internal callback for parsign a dummy stream
	/*!
		This is internally called when a deserialization destionation
		ostream could not be optained, or there was an error while
		writing to destination stream.
	*/
	static int parseDummyStream(Base &_rb, FncData &_rfd);
public:
	enum {IsSerializer = false};
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
		//idbg("push nonptr "<<_name);
		fstk.push(FncData(&Deserializer::parse<T>, (void*)&_t, _name));
		return *this;
	}
	
	template <typename T>
	Deserializer& push(T* &_t, const char *_name = NULL){
		//idbg("push ptr "<<_name);
		fstk.push(FncData(ptypeidf, &_t, _name));
		return *this;
	}
	//! Schedules a std (style) container for deserialization
	template <typename T>
	Deserializer& pushContainer(T &_t, const char *_name = NULL){
		fstk.push(FncData(&Deserializer::template parseContainer<T>, (void*)&_t, _name));
		return *this;
	}
	//! Schedules a std (style) container for deserialization
	template <typename T>
	Deserializer& pushContainer(T* &_t, const char *_name = NULL){
		cassert(!_t);//the pointer must be null!!
		fstk.push(FncData(&Deserializer::template parseContainer<T>, (void*)&_t, _name, 0));
		return *this;
	}
	//! Schedules the deserialization of an object containing streams.
	/*!
		Unfortunately this must be intrussive, that is the given streammer object must
		export a required interface.
		Please see the test/algorithm/serialization/fork.cpp example and/or 
		test::alpha::FetchSlaveCommand.
	*/
	template <typename T>
	Deserializer& pushStreammer(T *_p, const char *_name = NULL){
		//idbg("push special "<<_name);
		fstk.push(FncData(&Deserializer::template parseStreamBegin<T>, _p, _name, 0));
		return *this;
	}
private:
	FncTp		ptypeidf;
	const char	*pb;
	const char	*cpb;
	const char	*be;
	std::string	tmpstr;
};

}//namespace bin
}//namespace serialization


#endif
