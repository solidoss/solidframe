/* Declarations file binary.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ALGORITHM_SERIALIZATION_BINARY_HPP
#define ALGORITHM_SERIALIZATION__BINARY_HPP

#include <typeinfo>
#include <string>

#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "utility/common.hpp"
#include "utility/stack.hpp"
#include "algorithm/serialization/typemapper.hpp"

class IStream;
class OStream;

namespace serialization{
namespace bin{

BASIC_DECL(int8);
BASIC_DECL(uint8);
BASIC_DECL(int16);
BASIC_DECL(uint16);
BASIC_DECL(int32);
BASIC_DECL(uint32);
BASIC_DECL(int64);
BASIC_DECL(uint64);
//BASIC_DECL(int);

//! Nonintrusive string serialization/deserialization specification
template <class S>
S& operator&(std::string &_t, S &_s){
	return _s.push(_t, "string");
}

enum {
	MAXITSZ = sizeof(int64) + sizeof(int64) + sizeof(int64) + sizeof(int64),//!< Max sizeof(iterator) for serialized containers
	MINSTREAMBUFLEN = 128//if the free space for current buffer is less than this value
						//storring a stream will end up returning NOK
};
//===============================================================
//! A base class for binary serializer and deserializer
/*!
	The main goals for serializer and deserializer was to
	be ease to use and to be reentrant.
	The ease of use means that one should do little things to
	make a class serializable. E.g. :<br>
	<code>
	struct RemoteListCommand{<br>
	//...<br>
	template \< class S><br>
	S& operator&(S &_s){<br>
		_s.pushContainer(ppthlst, "strlst").push(err, "error").push(tout,"timeout");<br>
		_s.push(requid, "requid").push(strpth, "strpth").push(fromv, "from");<br>
		_s.push(cmduid.idx, "cmduid.idx").push(cmduid.uid,"cmduid.uid");<br>
		return _s;<br>
	}<br>
	//...<br>
	//data:<br>
	RemoteList::PathListT		*ppthlst;<br>
	String						strpth;<br>
	int							err;<br>
	uint32						tout;<br>
	fdt::ipc::ConnectionUid		conid;<br>
	fdt::ipc::CommandUid		cmduid;<br>
	uint32						requid;<br>
	ObjectUidT					fromv;<br>
	</code>
	<br>
	Reentrant means for serializer that:<br>
	* you push serializable objects onto the serializer<br>
	* you do a loop to actually serialize using a fixed size buffer
	until there is nothing to serialize:<br>
	<code>
	while((rv = ser.run(buf, blen)) == blen){<br>
		cnt += rv;<br>
		sock.write(buf, rv);<br>
	}<br>
	if(rv > 0){<br>
		sock.write(buf, blen);<br>
	}<br>
	</code>
	For deserializer means something equivalent:<br>
	* you push the serializable objects onto the deserializer<br>
	* you do a loop where you fed the deserializer, buffers filled
	e.g. from a file or a socket etc.
*/
class Base{
protected:
	struct FncData;
	typedef int (*FncT)(Base &, FncData &);
	//! Data associated to a callback
	struct FncData{
		FncData(
			FncT _f,
			void *_p,
			const char *_n = NULL,
			uint32 _s = -1
		):f(_f),p(_p),n(_n),s(_s){}
		
		FncT		f;	//!< Pointer to function
		void		*p;	//!< Pointer to data
		const char 	*n;	//!< Some name - of the item serialized
		uint32		s;	//!< Some size
	};
	
	struct ExtData{
		char 	buf[MAXITSZ];
		
		const uint32& u32()const{return *reinterpret_cast<const uint32*>(buf);}
		uint32& u32(){return *reinterpret_cast<uint32*>(buf);}
		const uint64& u64()const{return *reinterpret_cast<const uint64*>(buf);}
		uint64& u64(){return *reinterpret_cast<uint64*>(buf);}
		const int32& i32()const{return *reinterpret_cast<const int32*>(buf);}
		int32& i32(){return *reinterpret_cast<int32*>(buf);}
		int64& i64(){return *reinterpret_cast<int64*>(buf);}
		const int64& i64()const{return *reinterpret_cast<const int64*>(buf);}
		int64& i64_1(){return *reinterpret_cast<int64*>(buf + sizeof(int64));}
		const int64& i64_1()const{return *reinterpret_cast<const int64*>(buf + sizeof(int64));}
		void*& pv_2(){return *reinterpret_cast<void**>(buf + 2 * sizeof(int64));}
		const void* const& pv_2()const{return *reinterpret_cast<void const * const*>(buf + 2 * sizeof(int64));}
		
		ExtData(){}
		ExtData(uint32 _u32){u32() = _u32;}
		ExtData(int32 _i32){i32() = _i32;}
		ExtData(int64 _i64){i64() = _i64;}
		ExtData(int64 _i64_0, int64 _i64_1){i64() = _i64_0; i64_1() = _i64_1;}
		ExtData(int64 _i64_0, int64 _i64_1, void *_pv){
			i64() = _i64_0;
			i64_1() = _i64_1;
			pv_2() = _pv;
		}
	};
	struct IStreamData{
		IStreamData(
			IStream *_pis = NULL,
			int64 _sz = -1,
			uint64 _off = 0
		):pis(_pis), sz(_sz), off(_off){}
		IStream *pis;
		int64	sz;
		uint64	off;
	};
	struct OStreamData{
		OStreamData(
			OStream *_pos = NULL,
			int64 _sz = -1,
			uint64 _off = 0
		):pos(_pos), sz(_sz), off(_off){}
		OStream *pos;
		int64	sz;
		uint64	off;
	};
protected:
	//! Replace the top callback from the stack
	void replace(const FncData &_rfd);
	static int popEStack(Base &_rs, FncData &_rfd);
protected:
	typedef Stack<FncData>	FncDataStackT;
	typedef Stack<ExtData>	ExtDataStackT;
	ulong				ul;
	FncDataStackT		fstk;
	ExtDataStackT		estk;
};
//===============================================================
//! A fast reentrant binary serializer
/*!
	See serialization::bin::Base for details
*/
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
	static int store(Base &_rs, FncData &_rfd);
	
	static int storeBinary(Base &_rs, FncData &_rfd);
	
	template <typename T>
	static int storeContainer(Base &_rs, FncData &_rfd){
		idbgx(Dbg::ser_bin, "store generic container sizeof(iterator) = "<<sizeof(typename T::iterator)<<" "<<_rfd.n);
		Serializer &rs(static_cast<Serializer&>(_rs));
		if(!rs.cpb) return OK;
		T * c = reinterpret_cast<T*>(_rfd.p);
		if(c){
			cassert(sizeof(typename T::iterator) <= sizeof(ExtData));
			rs.estk.push(ExtData());
			typename T::iterator *pit(new(rs.estk.top().buf) typename T::iterator(c->begin()));
			//typename T::const_iterator &pit = *reinterpret_cast<typename T::const_iterator*>(estk.top().buf);
			*pit = c->begin();
			rs.estk.push(ExtData((int64)c->size()));
			_rfd.f = &Serializer::storeContainerContinue<T>;
			rs.fstk.push(FncData(&Base::popEStack, NULL));
			rs.fstk.push(FncData(&Serializer::template store<int64>, &rs.estk.top().i64()));
		}else{
			rs.estk.push(ExtData((int64)(-1)));
			rs.fstk.pop();
			rs.fstk.push(FncData(&Base::popEStack, NULL));
			rs.fstk.push(FncData(&Serializer::template store<int64>, &rs.estk.top().i64()));
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
		}
		//TODO:?!how
		//rit.T::~const_iterator();//only call the destructor
		rs.estk.pop();
		return OK;
	}
	
	template <typename T>
	static int storeArray(Base &_rs, FncData &_rfd){
		idbgx(Dbg::ser_bin, "store generic array "<<_rfd.n);
		Serializer &rs(static_cast<Serializer&>(_rs));
		if(!rs.cpb){
			rs.estk.pop();
			return OK;
		}
		T * c = reinterpret_cast<T*>(_rfd.p);
		if(c){
			_rfd.f = &Serializer::storeArrayContinue<T>;
			//rs.fstk.push(FncData(&Base::popEStack, NULL));
			rs.fstk.push(FncData(&Serializer::template store<int64>, &rs.estk.top().i64()));
		}else{
			rs.estk.top().i64() = -1;
			rs.fstk.pop();
			rs.fstk.push(FncData(&Base::popEStack, NULL));
			rs.fstk.push(FncData(&Serializer::template store<int64>, &rs.estk.top().i64()));
		}
		return CONTINUE;
	}
	
	template <typename T>
	static int storeArrayContinue(Base &_rs, FncData &_rfd){
		Serializer	&rs(static_cast<Serializer&>(_rs));
		T 			*c = reinterpret_cast<T*>(_rfd.p);
		const int64	&rsz(rs.estk.top().i64());
		int64 		&ri(rs.estk.top().i64_1());
		idbgx(Dbg::ser_bin, "store generic array cont "<<_rfd.n<<" rsz = "<<rsz<<" ri = "<<ri);
		
		if(rs.cpb && ri < rsz){
			rs.push(c[ri]);
			++ri;
			return CONTINUE;
		}
		//TODO:?!how
		//rit.T::~const_iterator();//only call the destructor
		rs.estk.pop();
		return OK;
	}
	
	template <typename T>
	static int storeStreamBegin(Base &_rs, FncData &_rfd){
		idbgx(Dbg::ser_bin, "store stream begin");
		Serializer &rs(static_cast<Serializer&>(_rs));
		if(!rs.cpb) return OK;
		T *pt = reinterpret_cast<T*>(_rfd.p);
		
		IStreamData	isd;
		int cpidx = _rfd.s;
		++_rfd.s;
		switch(pt->createSerializationStream(isd.pis, isd.sz, isd.off, cpidx)){
			case BAD: isd.sz = -1;break;//send -1
			case NOK: return OK;//no more streams
			case OK:
				cassert(isd.pis);
				cassert(isd.sz >= 0);
				break;
			default:
				cassert(false);
		}
		rs.estk.push(ExtData());
		IStreamData &rsp(*reinterpret_cast<IStreamData*>(rs.estk.top().buf));
		rsp = isd;
		//_rfd.s is the id of the stream
		rs.fstk.push(FncData(&Serializer::storeStreamDone<T>, _rfd.p, _rfd.n, _rfd.s - 1));
		if(rsp.pis){
			rs.fstk.push(FncData(&Serializer::storeStream, NULL));
		}
		rs.fstk.push(FncData(&Serializer::storeBinary, &rsp.sz, _rfd.n, sizeof(int64)));
		return CONTINUE;
	}
	template <typename T>
	static int storeStreamDone(Base &_rs, FncData &_rfd){
		idbgx(Dbg::ser_bin, "store stream done");
		Serializer &rs(static_cast<Serializer&>(_rs));
		IStreamData &rsp(*reinterpret_cast<IStreamData*>(rs.estk.top().buf));
		T *pt = reinterpret_cast<T*>(_rfd.p);
		pt->destroySerializationStream(rsp.pis, rsp.sz, rsp.off, _rfd.s);
		rs.estk.pop();
		return OK;
	}
	//! Internal callback for storing a stream
	static int storeStream(Base &_rs, FncData &_rfd);
public:
	enum {IsSerializer = true, IsDeserializer = false};
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
		fstk.push(FncData(&Serializer::template storeStreamBegin<T>, _p, _name, 0));
		return *this;
	}
	Serializer& pushBinary(void *_p, size_t _sz, const char *_name = NULL){
		fstk.push(FncData(&Serializer::storeBinary, _p, _name, _sz));
		return *this;
	}
	template <typename T, typename ST>
	Serializer& pushArray(T *_p, const ST &_rsz, const char *_name = NULL){
		fstk.push(FncData(&Serializer::template storeArray<T>, (void*)_p, _name));
		estk.push(ExtData((int64)_rsz, (int64)0));
		return *this;
	}
	template <typename T, typename ST>
	Serializer& pushDynamicArray(T* &_rp, const ST &_rsz, const char *_name = NULL){
		fstk.push(FncData(&Serializer::template storeArray<T>, (void*)_rp, _name));
		estk.push(ExtData((int64)_rsz, (int64)0));
		return *this;
	}
private:
	friend class TypeMapper;
	FncT		ptypeidf;
	char		*pb;
	char		*cpb;
	char		*be;
	std::string	tmpstr;
};
//===============================================================
template <>
int Serializer::store<int8>(Base &_rb, FncData &_rfd);
template <>
int Serializer::store<uint8>(Base &_rb, FncData &_rfd);
template <>
int Serializer::store<int16>(Base &_rb, FncData &_rfd);
template <>
int Serializer::store<uint16>(Base &_rb, FncData &_rfd);
template <>
int Serializer::store<int32>(Base &_rb, FncData &_rfd);
template <>
int Serializer::store<uint32>(Base &_rb, FncData &_rfd);
template <>
int Serializer::store<int64>(Base &_rb, FncData &_rfd);
template <>
int Serializer::store<uint64>(Base &_rb, FncData &_rfd);
template <>
int Serializer::store<std::string>(Base &_rb, FncData &_rfd);

template <typename T>
int Serializer::store(Base &_rs, FncData &_rfd){
	idbgx(Dbg::ser_bin, "store generic non pointer");
	Serializer &rs(static_cast<Serializer&>(_rs));
	if(!rs.cpb) return OK;
	T& rt = *((T*)_rfd.p);
	rs.fstk.pop();
	rt & rs;
	return CONTINUE;
}

//! A fast reentrant binary deserializer
/*!
	See serialization::bin::Base for details
*/
class Deserializer: public Base{
	template <typename T>
	static int parse(Base& _rd, FncData &_rfd);
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
		idbgx(Dbg::ser_bin, "parse generic non pointer container "<<_rfd.n);
		Deserializer &rd(static_cast<Deserializer&>(_rb));
		if(!rd.cpb) return OK;
		_rfd.f = &Deserializer::parseContainerBegin<T>;
		rd.estk.push(ExtData((int64)0));
		rd.fstk.push(FncData(&Deserializer::parse<int64>, &rd.estk.top().i64()));
		return CONTINUE;
	}
	template <typename T>
	static int parseContainerBegin(Base &_rb, FncData &_rfd){
		Deserializer	&rd(static_cast<Deserializer&>(_rb));
		
		if(!rd.cpb){
			rd.estk.pop();
			return OK;
		}
		
		const int64		i = rd.estk.top().i64();
		
		vdbgx(Dbg::ser_bin, "i = "<<i);
		
		//rd.estk.pop();
		if(i < 0){
			cassert(!_rfd.s);
			T **c = reinterpret_cast<T**>(_rfd.p);
			*c = NULL;
			return OK;
		}else if(!_rfd.s){
			T **c = reinterpret_cast<T**>(_rfd.p);
			vdbgx(Dbg::ser_bin, "");
			*c = new T;
			_rfd.p = *c;
		}
		if(i){
			_rfd.f = &Deserializer::parseContainerContinue<T>;
			_rfd.s = 0;//(uint32)i;
			return CONTINUE;
		}
		return OK;
	}
	template <typename T>
	static int parseContainerContinue(Base &_rb, FncData &_rfd){
		Deserializer &rd(static_cast<Deserializer&>(_rb));
		int64	&ri = rd.estk.top().i64();
		if(rd.cpb && ri){
			T *c = reinterpret_cast<T*>(_rfd.p);
			c->push_back(typename T::value_type());
			rd.push(c->back());
			--ri;
			return CONTINUE;	
		}
		rd.estk.pop();
		return OK;
	}
	
	template <typename T, typename ST>
	static int parseArray(Base &_rb, FncData &_rfd){
		idbgx(Dbg::ser_bin, "parse generic array");
		Deserializer &rd(static_cast<Deserializer&>(_rb));
		if(!rd.cpb){
			rd.estk.pop();
			return OK;
		}
		const int64	&rsz(rd.estk.top().i64());
		//int64		&ri(rd.estk.top().i64_1());
		ST			&rextsz(*reinterpret_cast<ST*>(rd.estk.top().pv_2()));
		
		
		if(rsz < 0){
			cassert(!_rfd.s);
			T **c = reinterpret_cast<T**>(_rfd.p);
			*c = NULL;
			rd.estk.pop();
			rextsz = 0;
			return OK;
		}else if(!_rfd.s){
			T **c = reinterpret_cast<T**>(_rfd.p);
			vdbgx(Dbg::ser_bin, "");
			*c = new T[rsz];
			_rfd.p = *c;
		}
		rextsz = rsz;
		_rfd.f = &Deserializer::parseArrayContinue<T>;
		return CONTINUE;
	}
	template <typename T>
	static int parseArrayContinue(Base &_rb, FncData &_rfd){
		idbgx(Dbg::ser_bin, "parse generic array continue");
		Deserializer &rd(static_cast<Deserializer&>(_rb));
		const int64	&rsz = rd.estk.top().i64();
		int64		&ri = rd.estk.top().i64_1();
		if(rd.cpb && ri < rsz){
			T *c = reinterpret_cast<T*>(_rfd.p);
			rd.push(c[ri]);
			++ri;
			return CONTINUE;	
		}
		rd.estk.pop();
		return OK;
	}
	
	template <typename T>
	static int parseStreamBegin(Base &_rb,FncData &_rfd){
		idbgx(Dbg::ser_bin, "parse stream begin");
		Deserializer &rd(static_cast<Deserializer&>(_rb));
		if(!rd.cpb) return OK;
		
		int cpidx = _rfd.s;
		++_rfd.s;
		OStreamData sp;
		T	*pt = reinterpret_cast<T*>(_rfd.p);
		//TODO: createDeserializationStream should be given the size of the stream
		switch(pt->createDeserializationStream(sp.pos, sp.sz, sp.off, cpidx)){
			case BAD:	
			case OK: 	break;
			case NOK:	return OK;
		}
		rd.estk.push(ExtData());
		OStreamData &rsp(*reinterpret_cast<OStreamData*>(rd.estk.top().buf));
		rsp = sp;
		//_rfd.s is the id of the stream
		rd.fstk.push(FncData(&Deserializer::template parseStreamDone<T>, _rfd.p, _rfd.n, _rfd.s - 1));
		if(sp.pos)
			rd.fstk.push(FncData(&Deserializer::parseStream, NULL));
		else
			rd.fstk.push(FncData(&Deserializer::parseDummyStream, NULL));
		//TODO: move this line - so it is called before parseStreamBegin
		rd.fstk.push(FncData(&Deserializer::parseBinary, &rsp.sz, _rfd.n, sizeof(int64)));
		return CONTINUE;
	}
	template <typename T>
	static int parseStreamDone(Base &_rb,FncData &_rfd){
		idbgx(Dbg::ser_bin, "parse stream done");
		Deserializer &rd(static_cast<Deserializer&>(_rb));
		OStreamData &rsp(*reinterpret_cast<OStreamData*>(rd.estk.top().buf));
		T	*pt = reinterpret_cast<T*>(_rfd.p);
		pt->destroyDeserializationStream(rsp.pos, rsp.sz, rsp.off, _rfd.s);
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
	enum {IsSerializer = false, IsDeserializer = true};
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
		fstk.push(FncData(&Deserializer::parse<T>, (void*)&_t, _name));
		return *this;
	}
	
	template <typename T>
	Deserializer& push(T* &_t, const char *_name = NULL){
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
		fstk.push(FncData(&Deserializer::template parseStreamBegin<T>, _p, _name, 0));
		return *this;
	}
	Deserializer& pushBinary(void *_p, size_t _sz, const char *_name = NULL){
		fstk.push(FncData(&Deserializer::parseBinary, _p, _name, _sz));
		return *this;
	}
	template <typename T, typename ST>
	Deserializer& pushArray(T* _p, ST &_rsz, const char *_name = NULL){
		fstk.push(FncData(&Deserializer::template parseArray<T, ST>, (void*)_p, _name));
		estk.push(ExtData((int64)0, (int64)0, (void*)&_rsz));
		fstk.push(FncData(&Deserializer::parse<int64>, &estk.top().i64()));
		return *this;
	}
	template <typename T, typename ST>
	Deserializer& pushDynamicArray(T* &_p, ST &_rsz, const char *_name = NULL){
		fstk.push(FncData(&Deserializer::template parseArray<T, ST>, (void*)&_p, _name, 0));
		estk.push(ExtData((int64)0, (int64)0, (void*)&_rsz));
		fstk.push(FncData(&Deserializer::parse<int64>, &estk.top().i64()));
		return *this;
	}
private:
	FncT		ptypeidf;
	const char	*pb;
	const char	*cpb;
	const char	*be;
	std::string	tmpstr;
};

template <>
int Deserializer::parse<int16>(Base &_rb, FncData &_rfd);
template <>
int Deserializer::parse<uint16>(Base &_rb, FncData &_rfd);
template <>
int Deserializer::parse<int32>(Base &_rb, FncData &_rfd);
template <>
int Deserializer::parse<uint32>(Base &_rb, FncData &_rfd);

template <>
int Deserializer::parse<int64>(Base &_rb, FncData &_rfd);
template <>
int Deserializer::parse<uint64>(Base &_rb, FncData &_rfd);
template <>
int Deserializer::parse<std::string>(Base &_rb, FncData &_rfd);

template <typename T>
int Deserializer::parse(Base& _rd, FncData &_rfd){
	idbgx(Dbg::ser_bin, "parse generic non pointer");
	Deserializer &rd(static_cast<Deserializer&>(_rd));
	if(!rd.cpb) return OK;
	rd.fstk.pop();
	*reinterpret_cast<T*>(_rfd.p) & rd;
	return CONTINUE;
}


}//namespace bin
}//namespace serialization


#endif
