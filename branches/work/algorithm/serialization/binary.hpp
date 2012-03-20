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
#define ALGORITHM_SERIALIZATION_BINARY_HPP

#include <typeinfo>
#include <string>

#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "utility/common.hpp"
#include "utility/stack.hpp"
#include "algorithm/serialization/typemapperbase.hpp"

class InputStream;
class OutputStream;

namespace serialization{
namespace binary{

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
	MINSTREAMBUFLEN = 16//if the free space for current buffer is less than this value
						//storring a stream will end up returning NOK
};

struct Limits{
	static Limits const& the();
	Limits():stringlimit(0), containerlimit(0), streamlimit(0){}//unlimited by default
	uint32 stringlimit;
	uint32 containerlimit;
	uint64 streamlimit;
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
public:
	static const char *errorString(const uint16 _err);
	void resetLimits(){
		limits = rdefaultlimits;
	}
	bool ok()const{return err == 0;}
	uint16 error()const{
		return err;
	}
	uint16 streamError()const{
		return streamerr;
	}
	const char* errorString()const{
		return errorString(err);
	}
	const char* streamErrorString()const{
		return errorString(streamerr);
	}
	void typeMapper(const TypeMapperBase &_rtm){
		ptm = &_rtm;
	}
	void pop(){
		fstk.pop();
	}
	uint64 const& streamSize()const{
		return streamsz;
	}
protected:
	enum Errors{
		ERR_NOERROR = 0,
		ERR_ARRAY_LIMIT,
		ERR_ARRAY_MAX_LIMIT,
		ERR_CONTAINER_LIMIT,
		ERR_CONTAINER_MAX_LIMIT,
		ERR_STREAM_LIMIT,
		ERR_STREAM_CHUNK_MAX_LIMIT,
		ERR_STREAM_SEEK,
		ERR_STREAM_READ,
		ERR_STREAM_WRITE,
		ERR_STREAM_SENDER,
		ERR_STRING_LIMIT,
		ERR_STRING_MAX_LIMIT,
		ERR_UTF8_LIMIT,
		ERR_UTF8_MAX_LIMIT,
		ERR_POINTER_UNKNOWN,
		ERR_REINIT,
	};
	struct FncData;
	typedef int (*FncT)(Base &, FncData &);
	//! Data associated to a callback
	struct FncData{
		FncData(
			FncT _f,
			void *_p,
			const char *_n = NULL,
			uint64 _s = -1
		):f(_f),p(_p),n(_n),s(_s){}
		
		FncT		f;	//!< Pointer to function
		void		*p;	//!< Pointer to data
		const char	*n;	//!< Some name - of the item serialized
		uint64		s;	//!< Some size
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
		ExtData(int32 _i32, int64 _i64_1){i32() = _i32; i64_1() = _i64_1;}
		ExtData(uint32 _u32, int64 _i64_1){u32() = _u32; i64_1() = _i64_1;}
		ExtData(int64 _i64){i64() = _i64;}
		ExtData(int64 _i64_0, int64 _i64_1){i64() = _i64_0; i64_1() = _i64_1;}
		ExtData(int64 _i64_0, int64 _i64_1, void *_pv){
			i64() = _i64_0;
			i64_1() = _i64_1;
			pv_2() = _pv;
		}
	};
protected:
	static int setStringLimit(Base& _rd, FncData &_rfd);
	static int setStreamLimit(Base& _rd, FncData &_rfd);
	static int setContainerLimit(Base& _rd, FncData &_rfd);

	Base():rdefaultlimits(Limits::the()), limits(rdefaultlimits), ptm(NULL){}
	Base(Limits const &_rdefaultlimits):rdefaultlimits(_rdefaultlimits), ptm(NULL){}
	//! Replace the top callback from the stack
	void replace(const FncData &_rfd);
	static int popEStack(Base &_rs, FncData &_rfd);
	const TypeMapperBase& typeMapper(){
		return *ptm;
	}
protected:
	typedef Stack<FncData>	FncDataStackT;
	typedef Stack<ExtData>	ExtDataStackT;
	const Limits			&rdefaultlimits;
	Limits					limits;
	const TypeMapperBase	*ptm;
	uint16					err;
	uint16					streamerr;
	uint64					streamsz;
	ulong					ul;
	FncDataStackT			fstk;
	ExtDataStackT			estk;
};
//===============================================================
//! A fast reentrant binary serializer
/*!
	See serialization::bin::Base for details
*/
class Serializer: public Base{
	template <uint S>
	static int storeBinary(Base &_rs, FncData &_rfd);
	
	template <typename T>
	static int store(Base &_rs, FncData &_rfd);
	
	static int storeUtf8(Base &_rs, FncData &_rfd);
	
	template <typename T>
	static int storeContainer(Base &_rs, FncData &_rfd){
		idbgx(Dbg::ser_bin, "store generic container sizeof(iterator) = "<<sizeof(typename T::iterator)<<" "<<_rfd.n);
		Serializer	&rs(static_cast<Serializer&>(_rs));
		if(!rs.cpb) return OK;
		T 			*c = reinterpret_cast<T*>(_rfd.p);
		const char	*n = _rfd.n;
		if(c){
			cassert(sizeof(typename T::iterator) <= sizeof(ExtData));
			if(rs.limits.containerlimit && c->size() > rs.limits.containerlimit){
				rs.err = ERR_CONTAINER_LIMIT;
				return BAD;
			}
			if(c->size() > CRCValue<uint32>::maximum()){
				rs.err = ERR_CONTAINER_MAX_LIMIT;
				return BAD;
			}
			rs.estk.push(ExtData());
			typename T::iterator *pit(new(rs.estk.top().buf) typename T::iterator(c->begin()));
			//typename T::const_iterator &pit = *reinterpret_cast<typename T::const_iterator*>(estk.top().buf);
			*pit = c->begin();
			const CRCValue<uint32>	crcsz(c->size());
			rs.estk.push(ExtData((uint32)crcsz));
			_rfd.f = &Serializer::storeContainerContinue<T>;
			rs.fstk.push(FncData(&Base::popEStack, NULL));
			rs.fstk.push(FncData(&Serializer::template store<uint32>, &rs.estk.top().u32(), n));
		}else{
			rs.estk.push(ExtData((int32)(-1)));
			rs.fstk.pop();
			rs.fstk.push(FncData(&Base::popEStack, NULL));
			rs.fstk.push(FncData(&Serializer::template store<uint32>, &rs.estk.top().u32(), n));
		}
		return CONTINUE;
	}
	
	template <typename T>
	static int storeContainerContinue(Base &_rs, FncData &_rfd){
		Serializer &rs(static_cast<Serializer&>(_rs));
		typename T::iterator &rit = *reinterpret_cast<typename T::iterator*>(rs.estk.top().buf);
		T * c = reinterpret_cast<T*>(_rfd.p);
		if(rs.cpb && rit != c->end()){
			rs.push(*rit, _rfd.n);
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
		Serializer	&rs(static_cast<Serializer&>(_rs));
		if(!rs.cpb){
			rs.estk.pop();
			return OK;
		}
		
		T			*c = reinterpret_cast<T*>(_rfd.p);
		const char	*n = _rfd.n;
		if(c && rs.estk.top().i32() != -1){
			if(rs.limits.containerlimit && rs.estk.top().u32() > rs.limits.containerlimit){
				rs.err = ERR_ARRAY_LIMIT;
				return BAD;
			}else if(rs.estk.top().u32() <= CRCValue<uint32>::maximum()){
				_rfd.f = &Serializer::storeArrayContinue<T>;
				const CRCValue<uint32>	crcsz(rs.estk.top().u32());
				rs.estk.push(ExtData((uint32)crcsz));
				rs.fstk.push(FncData(&Base::popEStack, NULL));
				rs.fstk.push(FncData(&Serializer::template store<uint32>, &rs.estk.top().u32(), n));
			}else{
				rs.err = ERR_ARRAY_MAX_LIMIT;
				return BAD;
			}
		}else{
			rs.estk.top().i32() = -1;
			rs.fstk.pop();
			rs.fstk.push(FncData(&Base::popEStack, NULL));
			rs.fstk.push(FncData(&Serializer::template store<int32>, &rs.estk.top().i32(), n));
		}
		return CONTINUE;
	}
	
	template <typename T>
	static int storeArrayContinue(Base &_rs, FncData &_rfd){
		Serializer	&rs(static_cast<Serializer&>(_rs));
		T 			*c = reinterpret_cast<T*>(_rfd.p);
		const int64	&rsz(rs.estk.top().i32());
		int64 		&ri(rs.estk.top().i64_1());
		idbgx(Dbg::ser_bin, "store generic array cont "<<_rfd.n<<" rsz = "<<rsz<<" ri = "<<ri);
		
		if(rs.cpb && ri < rsz){
			rs.push(c[ri], _rfd.n);
			++ri;
			return CONTINUE;
		}
		//TODO:?!how
		//rit.T::~const_iterator();//only call the destructor
		rs.estk.pop();
		return OK;
	}
	
	static int storeStreamBegin(Base &_rs, FncData &_rfd);
	static int storeStreamCheck(Base &_rs, FncData &_rfd);
	
	//! Internal callback for storing a stream
	static int storeStream(Base &_rs, FncData &_rfd);
	
	template <class T, uint32 I>
	static int storeReinit(Base &_rs, FncData &_rfd){
		Serializer		&rs(static_cast<Serializer&>(_rs));
		const uint32	val = _rfd.s;
		
		int rv = reinterpret_cast<T*>(_rfd.p)->template serializationReinit<Serializer, I>(rs, val);
		if(rv == BAD){
			rs.err = ERR_REINIT;
		}
		return rv;
	}
public:
	enum {IsSerializer = true, IsDeserializer = false};
	
	static char* storeValue(char *_pd, const uint16 _val);
	static char* storeValue(char *_pd, const uint32 _val);
	static char* storeValue(char *_pd, const uint64 _val);
	
	Serializer(
		const TypeMapperBase &_rtm
	):pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(ulong));
		typeMapper(_rtm);
	}
	Serializer(
		const TypeMapperBase &_rtm,
		Limits const & _rdefaultlimits
	):Base(_rdefaultlimits), pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(ulong));
		typeMapper(_rtm);
	}
	Serializer():pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(ulong));
	}
	Serializer(
		Limits const & _rdefaultlimits
	):Base(_rdefaultlimits), pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(ulong));
	}
	~Serializer();
	void clear();
	bool empty()const {return fstk.empty();}
	int run(char *_pb, unsigned _bl);
	
    Serializer& pushStringLimit();
	Serializer& pushStringLimit(uint32 _v);
	Serializer& pushStreamLimit();
	Serializer& pushStreamLimit(uint64 _v);
	Serializer& pushContainerLimit();
	Serializer& pushContainerLimit(uint32 _v);
	
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
		typeMapper().prepareStorePointer(this, _t, TypeMapperBase::typeName<T>(_t), _name);
		return *this;
	}
	template <typename T, typename TM, typename ID>
	Serializer& push(
		T* _t, const TM & _rtm,
		const ID &_rid, const char *_name = NULL
	){
		typeMapper().prepareStorePointer(this, _t, _rtm.realIdentifier(_rid), _name);
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
	Serializer& pushBinary(void *_p, size_t _sz, const char *_name = NULL);
	template <typename T, typename ST>
	Serializer& pushArray(T *_p, const ST &_rsz, const char *_name = NULL){
		fstk.push(FncData(&Serializer::template storeArray<T>, (void*)_p, _name));
		estk.push(ExtData((uint32)_rsz, (int64)0));
		return *this;
	}
	template <typename T, typename ST>
	Serializer& pushDynamicArray(
		T* &_rp, const ST &_rsz, const char *_name = NULL
	){
		fstk.push(FncData(&Serializer::template storeArray<T>, (void*)_rp, _name));
		estk.push(ExtData((int32)_rsz, (int64)0));
		return *this;
	}
	Serializer& pushUtf8(
		const std::string& _str, const char *_name = NULL
	);
	template <class T, uint32 I>
	Serializer& pushReinit(
		T *_pt, const uint64 &_rval = 0, const char *_name = NULL
	){
		fstk.push(FncData(&Serializer::template storeReinit<T, I>, _pt, _name, _rval));
		return *this;
	}
	Serializer& pushStream(
		InputStream *_ps, const char *_name = NULL
	);
	Serializer& pushStream(
		InputStream *_ps,
		const uint64 &_rfrom,
		const uint64 &_rlen,
		const char *_name = NULL
	);
	Serializer& pushStream(
		OutputStream *_ps, const char *_name = NULL
	);
	Serializer& pushStream(
		OutputStream *_ps,
		const uint64 &_rfrom,
		const uint64 &_rlen,
		const char *_name = NULL
	);
private:
	friend class TypeMapperBase;
	char					*pb;
	char					*cpb;
	char					*be;
	std::string				tmpstr;
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
	static int parseTypeIdDone(Base& _rd, FncData &_rfd);
	static int parseTypeId(Base& _rd, FncData &_rfd);
	
	template <typename T>
	static int parse(Base& _rd, FncData &_rfd);
	
	template <uint S>
	static int parseBinary(Base &_rb, FncData &_rfd);
	
	static int parseBinaryString(Base &_rb, FncData &_rfd);
	static int parseBinaryStringCheck(Base &_rb, FncData &_rfd);
	static int parseUtf8(Base &_rb, FncData &_rfd);
	
	template <typename T>
	static int parseContainer(Base &_rb, FncData &_rfd){
		idbgx(Dbg::ser_bin, "parse generic non pointer container "<<_rfd.n);
		Deserializer &rd = static_cast<Deserializer&>(_rb);
		if(!rd.cpb) return OK;
		_rfd.f = &Deserializer::parseContainerBegin<T>;
		rd.estk.push(ExtData((int64)0));
		rd.fstk.push(FncData(&Deserializer::parse<int32>, &rd.estk.top().i32()));
		return CONTINUE;
	}
	template <typename T>
	static int parseContainerBegin(Base &_rb, FncData &_rfd){
		Deserializer	&rd = static_cast<Deserializer&>(_rb);
		
		if(!rd.cpb){
			rd.estk.pop();
			return OK;
		}
		{
			const int32		i = rd.estk.top().i32();
			if(i != -1){
				const CRCValue<uint32> crcsz(CRCValue<uint32>::check_and_create((uint32)i));
				if(crcsz.ok()){
					rd.estk.top().i32() = crcsz.value();
				}else{
					rd.err = ERR_CONTAINER_MAX_LIMIT;
					return BAD;
				}
			}
		}
		const int32 i(rd.estk.top().i32());
		vdbgx(Dbg::ser_bin, "i = "<<i);
		
		if(
			i > 0 && static_cast<int32>(rd.limits.containerlimit) && 
			i > static_cast<int32>(rd.limits.containerlimit)
		){
			idbgx(Dbg::ser_bin, "error");
			rd.err = ERR_CONTAINER_LIMIT;
			return BAD;
		}
		
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
		int32	&ri = rd.estk.top().i32();
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
		{
			const int32	&rsz(rd.estk.top().i32());
			if(rsz != -1){
				const CRCValue<uint32> crcsz(CRCValue<uint32>::check_and_create((uint32)rsz));
				if(crcsz.ok()){
					rd.estk.top().i32() = crcsz.value();
				}else{
					rd.err = ERR_ARRAY_MAX_LIMIT;
					return BAD;
				}
			}
		}
		const int32	&rsz(rd.estk.top().i32());
		//int64		&ri(rd.estk.top().i64_1());
		ST			&rextsz(*reinterpret_cast<ST*>(rd.estk.top().pv_2()));
		if(rsz > 0 && rd.limits.containerlimit && rsz > static_cast<int32>(rd.limits.containerlimit)){
			idbgx(Dbg::ser_bin, "error");
			rd.err = ERR_ARRAY_LIMIT;
			return BAD;
		}
		
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
		idbgx(Dbg::ser_bin, "parse generic array continue "<<_rfd.n);
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
	
	//! Internal callback for parsing a stream
	static int parseStream(Base &_rb, FncData &_rfd);
	static int parseStreamBegin(Base &_rb, FncData &_rfd);
	static int parseStreamCheck(Base &_rb, FncData &_rfd);
	//! Internal callback for parsign a dummy stream
	/*!
		This is internally called when a deserialization destionation
		ostream could not be optained, or there was an error while
		writing to destination stream.
	*/
	static int parseDummyStream(Base &_rb, FncData &_rfd);
	
	template <class T, uint32 I>
	static int parseReinit(Base &_rb, FncData &_rfd){
		Deserializer	&rd(static_cast<Deserializer&>(_rb));
		const uint32	val = _rfd.s;
		
		int rv = reinterpret_cast<T*>(_rfd.p)->template serializationReinit<Deserializer, I>(rd, val);
		if(rv == BAD){
			rd.err = ERR_REINIT;
		}
		return rv;
	}
public:
	enum {IsSerializer = false, IsDeserializer = true};
	static const char* parseValue(const char *_ps, uint16 &_val);
	static const char* parseValue(const char *_ps, uint32 &_val);
	static const char* parseValue(const char *_ps, uint64 &_val);
	Deserializer(
		const TypeMapperBase &_rtm
	):pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(uint32));
		typeMapper(_rtm);
	}
	Deserializer(
		const TypeMapperBase &_rtm,
		Limits const & _rdefaultlimits
	):Base(_rdefaultlimits), pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(uint32));
		typeMapper(_rtm);
	}
	Deserializer():pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(uint32));
	}
	Deserializer(
		Limits const & _rdefaultlimits
	):Base(_rdefaultlimits), pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(uint32));
	}
	~Deserializer();
	void clear();
	bool empty()const {return fstk.empty();}
	int run(const char *_pb, unsigned _bl);
	
	Deserializer& pushStringLimit();
	Deserializer& pushStringLimit(uint32 _v);
	Deserializer& pushStreamLimit();
	Deserializer& pushStreamLimit(uint64 _v);
	Deserializer& pushContainerLimit();
	Deserializer& pushContainerLimit(uint32 _v);
	
	template <typename T>
	Deserializer& push(T &_t, const char *_name = NULL){
		fstk.push(FncData(&Deserializer::parse<T>, (void*)&_t, _name));
		return *this;
	}
	
	template <typename T>
	Deserializer& push(T* &_t, const char *_name = NULL){
		fstk.push(FncData(&Deserializer::parseTypeId, &_t, _name));
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
	Deserializer& pushBinary(void *_p, size_t _sz, const char *_name = NULL);
	
	template <typename T, typename ST>
	Deserializer& pushArray(T* _p, ST &_rsz, const char *_name = NULL){
		fstk.push(FncData(&Deserializer::template parseArray<T, ST>, (void*)_p, _name));
		estk.push(ExtData((int64)0, (int64)0, (void*)&_rsz));
		fstk.push(FncData(&Deserializer::parse<int32>, &estk.top().i32()));
		return *this;
	}
	template <typename T, typename ST>
	Deserializer& pushDynamicArray(T* &_p, ST &_rsz, const char *_name = NULL){
		fstk.push(FncData(&Deserializer::template parseArray<T, ST>, (void*)&_p, _name, 0));
		estk.push(ExtData((int64)0, (int64)0, (void*)&_rsz));
		fstk.push(FncData(&Deserializer::parse<int32>, &estk.top().i32()));
		return *this;
	}
	Deserializer& pushUtf8(
		std::string& _str, const char *_name = NULL
	);
	
	template <class T, uint32 I>
	Deserializer& pushReinit(
		T *_pt, const uint64 &_rval = 0, const char *_name = NULL
	){
		fstk.push(FncData(&Deserializer::template parseReinit<T, I>, _pt, _name, _rval));
		return *this;
	}
	Deserializer& pushStream(
		OutputStream *_ps, const char *_name = NULL
	);
	Deserializer& pushStream(
		OutputStream *_ps,
		const uint64 &_rat,
		const uint64 &,
		const char *_name = NULL
	);
	Deserializer& pushStream(
		InputStream *_ps, const char *_name = NULL
	);
	Deserializer& pushStream(
		InputStream *_ps,
		const uint64 &_rat,
		const uint64 &,
		const char *_name = NULL
	);
private:
	const char				*pb;
	const char				*cpb;
	const char				*be;
	std::string				tmpstr;
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


}//namespace binary
}//namespace serialization


#endif
