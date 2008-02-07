/* Declarations file binary.h
	
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

#ifndef SERIALIZATION_BINARY_H
#define SERIALIZATION_BINARY_H

#include <map>
#include <list>
#include <typeinfo>
#include <string>

#include "system/cassert.h"
#include "system/debug.h"
#include "utility/common.h"
#include "utility/stack.h"

#define SIMPLE_DECL(tp) \
template <class S>\
S& operator&(tp &_t, S &_s){\
	return _s.pushBinary(&_t, sizeof(tp), "binary");\
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
//! Comparator functor used for std::map
struct Cmp{
	bool operator()(const char *const &_s1, const char *const &_s2)const{
		return strcmp(_s1, _s2) < 0;
	}
};
//! Base class for serializer and deserializer
/*!
	Among other data it keeps two stacks 
	- one with callbacks and parameters
	- and one with extra data - usually iterators for serializing containers
*/
struct Base{
	struct FncData;
	typedef int (Base::*FncTp)(FncData &);
	//! Data associated to a callback
	struct FncData{
		FncData(FncTp _f, void *_p, const char *_n = NULL, ulong _s = -1):f(_f),p(_p),n(_n),s(_s){}
		FncTp		f;//!< Pointer to function
		void		*p;//!< Pointer to data
		const char 	*n;//!< Some name - of the item serialized
		ulong		s;//!< Some size
	};
protected:
	struct ExtData{
		char buf[MAXITSZ];
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
	//! Internal callback for storing binary data
	int storeBinary(FncData &_rfd);
	//! Internal callback for storing a stream
	int storeStream(FncData &_rfd);
	//! Internal callback for parsing binary data
	int parseBinary(FncData &_rfd);
	//! Internal callback for parsing a stream
	int parseStream(FncData &_rfd);
	//! Internal callback for parsign a dummy stream
	/*!
		This is internally called when a deserialization destionation
		ostream could not be optained, or there was an error while
		writing to destination stream.
	*/
	int parseDummyStream(FncData &_rfd);
	//! Pops from extended stack
	int popEStack(FncData &_rfd);
protected:
	typedef Stack<FncData>	FncDataStackTp;
	typedef Stack<ExtData>	ExtDataStackTp;
	typedef Stack<uint64>	UintStackTp;
	union StrConst{
		const char	*pc;
		char		*ps;
	};
	StrConst 			pb;
	StrConst 			cpb;
	StrConst			be;//buf end
	ulong				ul;
	//UintStackTp			ustk;
	FncDataStackTp		fstk;
	ExtDataStackTp		estk;	
};

class RTTIMapper;
#define REINTERPRET_FUN(fun) \
	reinterpret_cast<Base::FncTp>((tpf = &(fun)))
//! A binary serializer class with a given Mapper
/*!
	<b>Overview:</b><br>
	- It is buffer oriented not stream oriented so it is switable for asynchrounous usage.
	- It can be used with for serializing structures instrusively or non intrusively 
		(one can give external functions to specify how a structure can be serialized)
	- It can serialize (std)containers (with the constraint that 
		sizeof(container::iterator) <= MAXITSZ).
	- It  can serialize std strings
	- It can serialize streams (well not those from std but IStream from solidground 
		system library)
	- It uses the mapper to properly serialize a virtual base object, provided the 
		fact that the actual type of the object was registered into the mapper.
	<br>
	<b>Usage:</b><br>
	\see test/algorithm/serialization/simple.cpp test/algorithm/serialization/fork.cpp
	
	<b>Notes:</b><br>
	- There is one visible shortcomming for scheduling object serialization - one cannot
	(for now) do serialization time decisions for continuation, based on what was
	already serialized.
	
*/
template <class Mapper = RTTIMapper>
class Serializer: private Base{
	friend class RTTIMapper;
	typedef Base::FncData	FncData;
	typedef int (Serializer<Mapper>::*FncTp)(FncData &);
	template <class T>
	int storePointer(FncData &_rfd){
		idbg("store generic pointer");
		if(!cpb.ps) return OK;
		const char *tid = _rfd.n;
		uint32		sz = _rfd.s;
		estk.push(ExtData(_rfd.s));
		fstk.pop();
		**static_cast<T**>(_rfd.p) & (*this);
		FncTp	tpf;
		idbg("tid = "<<tid<<" sz = "<<sz);
		fstk.push(FncData(REINTERPRET_FUN(Serializer::storeBinary), (void*)tid, NULL, sz));
		fstk.push(FncData(&Base::popEStack, NULL));
		fstk.push(FncData(REINTERPRET_FUN(Serializer::template store<uint32>), &estk.top().u32()));
		return CONTINUE;
	}
	template <typename T>
	int store(FncData &_rfd){
		idbg("store generic non pointer");
		if(!cpb.ps) return OK;
		T& rt = *((T*)_rfd.p);
		fstk.pop();
		rt & (*this);
		return CONTINUE;
	}
	
	template <typename T>
	int storeContainer(FncData &_rfd){
		idbg("store generic non pointer container sizeof(iterator) = "<<sizeof(typename T::iterator));
		if(!cpb.ps) return OK;
		T * c = reinterpret_cast<T*>(_rfd.p);
		cassert(sizeof(typename T::iterator) <= sizeof(ExtData));
		estk.push(ExtData());
		typename T::iterator *pit(new(estk.top().buf) typename T::iterator(c->begin()));
		//typename T::const_iterator &pit = *reinterpret_cast<typename T::const_iterator*>(estk.top().buf);
		*pit = c->begin();
		estk.push(ExtData(c->size()));
		FncTp	tpf;
		_rfd.f = REINTERPRET_FUN(Serializer::template storeContainerContinue<T>);
		fstk.push(FncData(&Base::popEStack, NULL));
		fstk.push(FncData(REINTERPRET_FUN(Serializer::template store<uint32>), &estk.top().u32()));
		return CONTINUE;
	}
	
	template <typename T>
	int storeContainerContinue(FncData &_rfd){
		typename T::iterator &rit = *reinterpret_cast<typename T::iterator*>(estk.top().buf);
		T * c = reinterpret_cast<T*>(_rfd.p);
		if(cpb.ps && rit != c->end()){
			this->push(*rit);
			++rit;
			return CONTINUE;
		}else{
			//TODO:?!how
			//rit.T::~const_iterator();//only call the destructor
			estk.pop();
			return OK;
		}
	}
	
	template <typename T>
	int storeString(FncData &_rfd){
		idbg("store generic non pointer string");
		if(!cpb.ps) return OK;
		T * c = reinterpret_cast<T*>(_rfd.p);
		estk.push(ExtData(c->size()));
		FncTp	tpf;
		replace(FncData(REINTERPRET_FUN(Serializer::storeBinary), (void*)c->data(), _rfd.n, c->size()));
		fstk.push(FncData(&Base::popEStack, NULL));
		fstk.push(FncData(REINTERPRET_FUN(Serializer::template store<uint32>), &estk.top().u32()));
		return CONTINUE;
	}
	template <typename T>
	int storeStreamBegin(FncData &_rfd){
		idbg("store stream begin");
		if(!cpb.ps) return OK;
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
		estk.push(ExtData());
		std::pair<IStream*, int64> &rsp(*reinterpret_cast<std::pair<IStream*, int64>*>(estk.top().buf));
		rsp = sp;
		FncTp	tpf;
		//_rfd.s is the id of the stream
		fstk.push(FncData(REINTERPRET_FUN(Serializer::template storeStreamDone<T>), _rfd.p, _rfd.n, _rfd.s - 1));
		if(sp.first){
			fstk.push(FncData(REINTERPRET_FUN(Serializer::storeStream), NULL));
		}
		fstk.push(FncData(REINTERPRET_FUN(Serializer::storeBinary), &rsp.second, _rfd.n, sizeof(int64)));
		return CONTINUE;
	}
	template <typename T>
	int storeStreamDone(FncData &_rfd){
		idbg("store stream done");
		std::pair<IStream*, int64> &rsp(*reinterpret_cast<std::pair<IStream*, int64>*>(estk.top().buf));
		T *pt = reinterpret_cast<T*>(_rfd.p);
		pt->destroySerializationStream(rsp, _rfd.s);
		estk.pop();
		return OK;
	}
public:
	
	typedef Serializer<Mapper>	SerializerTp;
	//!Constructor with a Mapper
	explicit Serializer(Mapper &_rfm):rfm(_rfm){}
	//! The loop doing the actual serialization
	/*!
		The method will try to fill as much of the given buffer with data.
		It will return the lenght of the written data or < 0 in case of 
		error (actually this must be an exception because for reliability
		the serializer should be capable to recover from different errors).
		
		Consider using empty() to find out if there are items to serialize.
		
		\param _pb Pointer to a buffer the data should be serialized to
		\param _bl The length of the buffer
	*/
	int run(char *_pb, unsigned _bl){
		cpb.ps = pb.ps = _pb;
		be.ps = cpb.ps + _bl;
		while(fstk.size()){
			FncData &rfd = fstk.top();
			switch((this->*reinterpret_cast<FncTp>(rfd.f))(rfd)) {
				case CONTINUE: continue;
				case OK: fstk.pop(); break;
				case NOK: goto Done;
				case BAD: return -1;
			}
		}
		Done:
		return cpb.ps - pb.ps;
	}
	//! Destructor
	~Serializer(){
		run(NULL,0);//release the stacks
		cassert(!fstk.size());
	}
	//! Clear the stacks
	void clear(){
		run(NULL, 0);
	}
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
		FncTp	tpf;
		fstk.push(FncData(REINTERPRET_FUN(Serializer<Mapper>::template store<T>), (void*)&_t, _name));
		return *this;
	}
	//! Schedule a non pointer object for serialization
	/*!
		The object is only scheduled for serialization so it must remain in memory
		up until the serialization will end.
		
		The given name is meaningless for binary serialization, it will be usefull for
		text oriented serialization, and I want a common interface for push, so one
		can write a single template function for serializing an object.
	*/
	template <typename T>
	Serializer& push(T* &_t, const char *_name = NULL){
		//idbg("push ptr "<<_name);
		std::pair<const char*, size_t> ps;
		FncTp pf = (FncTp)rfm.template map(_t, &ps);
		cassert(pf);
		//TODO: remove the assert!!
		//pf = &Serializer::template storePointer<T>;
		fstk.push(FncData((Base::FncTp)pf, &_t, ps.first, ps.second));
		return *this;
	}
	/*
		Push a container that follows the stl interface.
		TODO: put here what interface it needs
	*/
	//! Schedules a stl style container for serialization
	template <typename T>
	Serializer& pushContainer(T &_t, const char *_name = NULL){
		//idbg("push nonptr "<<_name);
		FncTp	tpf;
		fstk.push(FncData(REINTERPRET_FUN(Serializer::template storeContainer<T>), (void*)&_t, _name));
		return *this;
	}
	//! Schedules a stl style string for serialization
	template <typename T>
	Serializer& pushString(T &_t, const char *_name = NULL){
		//idbg("push nonptr "<<_name);
		FncTp	tpf;
		fstk.push(FncData(REINTERPRET_FUN(Serializer::template storeString<T>), (void*)&_t, _name));
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
		FncTp	tpf;
		fstk.push(FncData(REINTERPRET_FUN(Serializer::template storeStreamBegin<T>), _p, _name, 0));
		return *this;
	}
	//! Schedules flat binary data for serialization.
	/*!
		Please use this with care.
	*/
	Serializer& pushBinary(void *_pv, size_t _sz, const char *_name = NULL){
		FncTp	tpf;
		fstk.push(FncData(REINTERPRET_FUN(Serializer::storeBinary), _pv, _name, _sz));
		return *this;
	}
	//! Checks if there is something scheduled for serialization
	bool empty()const{return fstk.empty();}
private:
	Mapper		&rfm;
};

//*****************************************************************************************	
//! A binary deserializer class with a given Mapper
/*!
	<b>Overview:</b><br>
	- It is buffer oriented not stream oriented so it is switable for asynchrounous usage.
	- It can be used with for deserializing structures instrusively or non intrusively 
		(one can give external functions to specify how a structure can be deserialized)
	- It can deserialize (std)containers (with the constraint that 
		sizeof(container::iterator) <= MAXITSZ).
	- It  can deserialize std strings
	- It can deserialize streams (well not those from std but IStream from solidground 
		system library)
	- It uses the mapper to properly deserialize a virtual base object, provided the 
		fact that the actual type of the object was registered into the mapper.
	<br>
	<b>Usage:</b><br>
	\see test/algorithm/serialization/simple.cpp test/algorithm/serialization/fork.cpp
	
	<b>Notes:</b><br>
	- There is one visible shortcomming for scheduling object deserialization - one cannot
	(for now) do deserialization time decisions for continuation, based on what was
	already deserialized.
	
*/
template <class Mapper = RTTIMapper>
class Deserializer: private Base{
	friend class RTTIMapper;
	typedef int (Deserializer::*FncTp)(FncData &);
	template <typename T>
	int parse(FncData &_rfd){
		idbg("parse generic non pointer");
		if(!cpb.ps) return OK;
		fstk.pop();
		*reinterpret_cast<T*>(_rfd.p) & (*this);
		return CONTINUE;
	}
	template <typename T>
	int parsePointer(FncData &_rfd){
		if(!cpb.ps) return OK;
		idbg("parse generic pointer");
		fstk.pop();
		T * pt;
		*reinterpret_cast<T**>(_rfd.p) = pt = new T;
		*pt & (*this);
		return CONTINUE;
	}
	int parsePointerBegin(FncData &_rfd){
		if(!cpb.ps) return OK;
		++slstit;
		if(slstit == slst.end()){
			slstit = slst.insert(slst.end(), "");
		}else{
			slstit->clear();
		}
		FncTp	tpf;
		_rfd.f = REINTERPRET_FUN(Deserializer::parsePointerContinue);
		fstk.push(FncData(REINTERPRET_FUN(Deserializer::template parseString<std::string>),&*slstit));
		return CONTINUE;
	}
	int parsePointerContinue(FncData &_rfd){
		idbg("parse pointer continue :"<<*slstit);
		if(!cpb.ps) return OK;
		//rfm.map(*slstit);
		const char *nm = slstit->c_str();
		_rfd.f = rfm.map(*slstit);
		idbg("name = "<<nm);
		--slstit;
		cassert(_rfd.f);
		return CONTINUE;
	}
	template <typename T>
	int parseContainer(FncData &_rfd){
		idbg("parse generic non pointer container");
		if(!cpb.ps) return OK;
		FncTp tpf;
		_rfd.f = REINTERPRET_FUN(Deserializer::template parseContainerBegin<T>);
		estk.push(ExtData(0));
		fstk.push(FncData(REINTERPRET_FUN(Deserializer::template parse<uint32>), &estk.top().u32()));
		return CONTINUE;
	}
	template <typename T>
	int parseContainerBegin(FncData &_rfd){
		if(!cpb.ps) return OK;
		uint32 ul = estk.top().u32();
		estk.pop();
		if(ul){
			FncTp tpf;
			_rfd.f = REINTERPRET_FUN(Deserializer::template parseContainerContinue<T>);
			_rfd.s = ul;
			return CONTINUE;
		}
		return OK;
	}
	template <typename T>
	int parseContainerContinue(FncData &_rfd){
		if(cpb.ps && _rfd.s){
			T *c = reinterpret_cast<T*>(_rfd.p);
			c->push_back(typename T::value_type());
			this->push(c->back());
			--_rfd.s;
			return CONTINUE;	
		}
		return OK;
	}
	template <typename T>
	int parseString(FncData &_rfd){
		idbg("parse generic non pointer string");
		if(!cpb.ps) return OK;
		FncTp tpf;
		estk.push(ExtData(0));
		replace(FncData(REINTERPRET_FUN(Deserializer::template parseBinaryString<T>), _rfd.p, _rfd.n));
		fstk.push(FncData(REINTERPRET_FUN(Deserializer::template parse<uint32>), &estk.top().u32()));
		return CONTINUE;
	}
	template <typename T>
	int parseStreamBegin(FncData &_rfd){
		idbg("parse stream begin");
		if(!cpb.ps) return OK;
		
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
		estk.push(ExtData());
		std::pair<OStream*, int64> &rsp(*reinterpret_cast<std::pair<OStream*, int64>*>(estk.top().buf));
		rsp = sp;
		FncTp tpf;
		fstk.push(FncData(REINTERPRET_FUN(Deserializer::template parseStreamDone<T>), _rfd.p, _rfd.n, _rfd.s - 1));//_rfd.s is the id of the stream
		if(sp.first)
			fstk.push(FncData(REINTERPRET_FUN(Deserializer::parseStream), NULL));
		else
			fstk.push(FncData(REINTERPRET_FUN(Deserializer::parseDummyStream), NULL));
		//TODO: move this line - so it is called before parseStreamBegin
		fstk.push(FncData(REINTERPRET_FUN(Deserializer::parseBinary), &rsp.second, _rfd.n, sizeof(int64)));
		return CONTINUE;
	}
	template <typename T>
	int parseStreamDone(FncData &_rfd){
		idbg("store stream done");
		std::pair<OStream*, int64> &rsp(*reinterpret_cast<std::pair<OStream*, int64>*>(estk.top().buf));
		T	*pt = reinterpret_cast<T*>(_rfd.p);
		pt->destroyDeserializationStream(rsp, _rfd.s);
		estk.pop();
		return OK;
	}
	template <class Str>
	int parseBinaryString(FncData &_rfd){
		if(!cpb.ps){
			estk.pop();
			return OK;
		}
		unsigned len = be.pc - cpb.pc;
		uint32 ul = estk.top().u32();
		if(len > ul) len = ul;
		Str *ps = reinterpret_cast<Str*>(_rfd.p);
		ps->append(cpb.pc, len);
		cpb.pc += len;
		ul -= len;
		if(ul){estk.top().u32() = ul; return NOK;}
		estk.pop();
		return OK;
	}

public:
	//! Constructor with the mapper
	explicit Deserializer(Mapper &_rfm):rfm(_rfm){
		slst.push_back("");
		slstit = slst.begin();
	}
	
	~Deserializer(){
		run(NULL, 0);
		cassert(!fstk.size());
	}
	//! Clears the callback stack
	void clear(){
		run(NULL, 0);
	}
	//! Check if there are scheduled objects for deserialization
	bool empty()const {return fstk.empty();}
	//! Schedule a non pointer object for deserialization
	/*!
		The object is only scheduled for deserialization so it must remain in memory
		up until the serialization will end.
		
		The given name is meaningless for binary deserialization, it will be usefull for
		text oriented serialization, and I want a common interface for push, so one
		can write a single template function for serializing an object.
	*/
	template <typename T>
	Deserializer& push(T &_t, const char *_name = NULL){
		//idbg("push nonptr "<<_name);
		FncTp tpf;
		fstk.push(FncData(REINTERPRET_FUN(Deserializer::template parse<T>), (void*)&_t, _name));
		return *this;
	}
	//! Schedule a pointer object for deserialization
	/*!
		A new object will be created using the default constructor (soo it must be available).
		Also the type of the given pointer may not be the type of the actual constructed object.
		
		The given name is meaningless for binary deserialization, it will be usefull for
		text oriented serialization, and I want a common interface for push, so one
		can write a single template function for serializing an object.
	*/
	template <typename T>
	Deserializer& push(T* &_t, const char *_name = NULL){
		//idbg("push ptr "<<_name);
		//FncTp pf = (FncTp)rfm.findO(typeid(*_t).name());
		//cassert(pf);
		//NOTE: you can continue with this line instead of assert:
		//pf = &Deserializer::template parsePointer<T>;
		FncTp tpf;
		fstk.push(FncData(REINTERPRET_FUN(Deserializer::parsePointerBegin), &_t, _name));
		return *this;
	}
	//! Schedules a std (style) container for deserialization
	template <typename T>
	Deserializer& pushContainer(T &_t, const char *_name = NULL){
		//idbg("push nonptr continer"<<_name);
		FncTp tpf;
		fstk.push(FncData(REINTERPRET_FUN(Deserializer::template parseContainer<T>), (void*)&_t, _name));
		return *this;
	}
	//! Schedules a std (style) string for deserialization
	template <typename T>
	Deserializer& pushString(T &_t, const char *_name = NULL){
		//idbg("push string"<<_name);
		FncTp tpf;
		fstk.push(FncData(REINTERPRET_FUN(Deserializer::template parseString<T>), (void*)&_t, _name));
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
		FncTp tpf;
		fstk.push(FncData(REINTERPRET_FUN(Deserializer::template parseStreamBegin<T>), _p, _name, 0));
		return *this;
	}
	
	//! The loop doing the actual deserialization
	/*!
		The method will try to use as much of the given data as it can.
		It will return the lenght of the consummed data or < 0 in case of 
		error (actually this must be an exception because for reliability
		the deserializer should be capable to recover from different errors).
		
		Consider using empty to find out if there are items to deserialize.
		
		\param _pb Pointer to a buffer the items should be deserialized from
		\param _bl The length of the buffer
	*/
	int run(const char *_pb, unsigned _bl){
		cpb.pc = pb.pc = _pb;
		be.pc = pb.pc + _bl;
		while(fstk.size()){
			FncData &rfd = fstk.top();
			switch((this->*reinterpret_cast<FncTp>(rfd.f))(rfd)){
				case CONTINUE: continue;
				case OK: fstk.pop(); break;
				case NOK: goto Done;
				case BAD: return -1;
			}
		}
		Done:
		return cpb.pc - pb.pc;
	}
	//! Schedules flat binary data for deserialization.
	/*!
		Please use this with care.
	*/
	Deserializer& pushBinary(void *_pv, size_t _sz, const char *_name = NULL){
		FncTp	tpf;
		fstk.push(FncData(REINTERPRET_FUN(Deserializer::parseBinary), _pv, _name, _sz));
		return *this;
	}
private:
	typedef std::list<std::string>	StrListTp;
	StrListTp				slst;
	StrListTp::iterator		slstit;
	Mapper					&rfm;
};


/*SIMPLE_DECL(int);
SIMPLE_DECL(uint);*/
SIMPLE_DECL(int16);
SIMPLE_DECL(uint16);
SIMPLE_DECL(int32);
SIMPLE_DECL(uint32);
SIMPLE_DECL(int64);
SIMPLE_DECL(uint64);

//! Nonintrusive string serialization/deserialization specification
template <class S>
S& operator&(std::string &_t, S &_s){
	return _s.pushString(_t, "string");
}


// struct FunctionMap: FunctionMapBase{
// 	template <class T>
// 	void insert(){
// 		//Serializer::FncTp fi = &Serializer::template storePointer<T>;
// 		Deserializer::FncTp fo = &Deserializer::template parsePointer<T>;
// 		lock();
// 		map(typeid(T).name(), (FncTp)fi, (FncTp)fo);
// 		unlock();
// 	}
// };
//! A type mapper for binary serialization usign runtime type identification
/*!
	Basically it maps types to the coresponding serialization/deserialization methods
	of serializer an deserializer. Also provides for internal deserialization usage,
	way to identify a type by its typeid.name and for serializtion provide a way to
	identify the real type of a pointer.
	
	<b>Usage:</b><br>
	Usually you only need one per application. Register the types of virtual clases.
	Then use the serializer and deserializer as explaine above. 
	\see test/algorithm/serialization/simple.cpp test/algorithm/serialization/fork.cpp
	
*/
struct RTTIMapper{
	RTTIMapper();
	~RTTIMapper();
	void lock();
	void unlock();
	//! Register a type
	template <class T>
	void map(){
		Serializer<RTTIMapper>::FncTp	pfi = &Serializer<RTTIMapper>::template storePointer<T>;
		Deserializer<RTTIMapper>::FncTp	pfo = &Deserializer<RTTIMapper>::template parsePointer<T>;
		Base::FncTp fi = (Base::FncTp)pfi;
		Base::FncTp fo = (Base::FncTp)pfo;
		lock();
		//const char *nm = typeid(T).name();
		m[typeid(T).name()] = FncPairTp(fi, fo);
		unlock();
	}
	//! Used by serializer to identify the type and type identifier for a pointer
	template <class T>
	Base::FncTp map(T *_p, std::pair<const char*, size_t> *_ps){
		Serializer<RTTIMapper>::FncTp	pf;
		cassert(_ps);
		_ps->first = typeid(*_p).name();
		_ps->second = strlen(_ps->first);
		lock();
		FunctionMapTp::iterator it = m.find(_ps->first);
		if(it != m.end()){
			pf = it->second.first;
			unlock();
			return (Base::FncTp)pf;
		}
		unlock();
		return NULL;
	}
	//! Used by deserializer to identify a type.
	Base::FncTp map(const std::string &_s);
private:
	typedef std::pair<Base::FncTp, Base::FncTp>		FncPairTp;
	typedef std::map<const char*, FncPairTp, Cmp> 	FunctionMapTp;
	FunctionMapTp	m;
	Mutex			*pmut;
};

}//namespace bin

}//namespace serialization


#endif
