// serialization/binary.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_SERIALIZATION_BINARY_HPP
#define SOLID_SERIALIZATION_BINARY_HPP

#include <typeinfo>
#include <string>

#include <istream>
#include <ostream>
#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "utility/common.hpp"
#include "utility/algorithm.hpp"
#include "utility/stack.hpp"
#include "utility/dynamicpointer.hpp"
#include "serialization/typemapperbase.hpp"

namespace solid{
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
BASIC_DECL(std::string);

//! Nonintrusive string serialization/deserialization specification
// template <class S>
// S& operator&(std::string &_t, S &_s){
// 	return _s.push(_t, "string");
// }

template <class S, class T>
void serialize(S &_s, T &_t){
	_t.serialize(_s);
}
template <class S, class Ctx, class T>
void serialize(S &_s, T &_t, Ctx &_ctx){
	_t.serialize(_s, _ctx);
}

enum {
	MAXITSZ = sizeof(int64) + sizeof(int64) + sizeof(int64) + sizeof(int64),//!< Max sizeof(iterator) for serialized containers
	MINSTREAMBUFLEN = 16//if the free space for current buffer is less than this value
						//storring a stream will end up returning Wait
};

enum CbkReturnValueE{
	Success,
	Wait,
	Failure,
	Continue,
	LastReturnValue,
};

struct Limits{
	static Limits const& the();
	Limits():stringlimit(-1), containerlimit(-1), streamlimit(-1){}//unlimited by default
	size_t stringlimit;
	size_t containerlimit;
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
		lmts = rdefaultlmts;
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
	Limits& limits(){
		return lmts;
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
	typedef CbkReturnValueE (*FncT)(Base &, FncData &, void*);
	//! Data associated to a callback
	struct FncData{

		FncData(
                        FncT _f,
                        const void *_p,
                        const char *_n = NULL,
                        uint64 _s = -1
                ):f(_f),p(const_cast<void*>(_p)),n(_n),s(_s){}
		
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
		uint64& u64_1(){return *reinterpret_cast<uint64*>(buf + sizeof(uint64));}
		const uint64& u64_1()const{return *reinterpret_cast<const uint64*>(buf + sizeof(uint64));}
		void*& pv_2(){return *reinterpret_cast<void**>(buf + 2 * sizeof(int64));}
		const void* const& pv_2()const{return *reinterpret_cast<void const * const*>(buf + 2 * sizeof(int64));}
		const void*const& p()const{
			return *reinterpret_cast<const void*const*>(buf);
		}
		void*& p(){
			return *reinterpret_cast<void**>(buf);
		}
		ExtData(){}
		
		ExtData(uint32 _u32){u32() = _u32;}
		ExtData(int32 _i32){i32() = _i32;}
		ExtData(uint64 _u64){u64() = _u64;}
		ExtData(uint64 _u64, uint64 _u64_1){u64() = _u64; u64_1() = _u64_1;}
		ExtData(void *_p){p() = _p;}
		ExtData(int32 _i32, int64 _i64_1){i32() = _i32; i64_1() = _i64_1;}
		ExtData(uint32 _u32, int64 _i64_1){u32() = _u32; i64_1() = _i64_1;}
		ExtData(int64 _i64){i64() = _i64;}
		ExtData(int64 _i64_0, int64 _i64_1){i64() = _i64_0; i64_1() = _i64_1;}
		ExtData(uint64 _u64_0, uint64 _u64_1, void *_pv){
			u64() = _u64_0;
			u64_1() = _u64_1;
			pv_2() = _pv;
		}
	};
protected:
	static CbkReturnValueE setStringLimit(Base& _rd, FncData &_rfd, void */*_pctx*/);
	static CbkReturnValueE setStreamLimit(Base& _rd, FncData &_rfd, void */*_pctx*/);
	static CbkReturnValueE setContainerLimit(Base& _rd, FncData &_rfd, void */*_pctx*/);

	Base():rdefaultlmts(Limits::the()), lmts(rdefaultlmts), ptm(NULL){}
	Base(Limits const &_rdefaultlmts):rdefaultlmts(_rdefaultlmts), lmts(rdefaultlmts), ptm(NULL){}
	//! Replace the top callback from the stack
	void replace(const FncData &_rfd);
	static CbkReturnValueE popEStack(Base &_rs, FncData &_rfd, void */*_pctx*/);
	const TypeMapperBase& typeMapper()const{
		return *ptm;
	}
	template <class T, class Ser, class H, class Ctx>
	static CbkReturnValueE handle(Base &_rs, FncData &_rfd, void *_pctx){
		idbgx(Debug::ser_bin, "Handle");
		Ser						&rs(static_cast<Ser&>(_rs));
		if(!rs.cpb) return Success;
		H						h;
		typename Ser::ContextT	&rctx = *reinterpret_cast<typename Ser::ContextT*>(_pctx);
		T						&rt = *((T*)_rfd.p);
		rs.fstk.pop();
		h(rs, rt, rctx);
		return Continue;
	}
protected:
	typedef Stack<FncData>	FncDataStackT;
	typedef Stack<ExtData>	ExtDataStackT;
	const Limits			&rdefaultlmts;
	Limits					lmts;
	const TypeMapperBase	*ptm;
	uint16					err;
	uint16					streamerr;
	uint64					streamsz;
	ulong					ul;
	FncDataStackT			fstk;
	ExtDataStackT			estk;
};

//===============================================================
template <class T>
struct SerializerPushHelper;
//! A reentrant binary serializer
/*!
	See serialization::bin::Base for details
*/
class SerializerBase: public Base{
protected:
	
	template <uint S>
	static CbkReturnValueE storeBinary(Base &_rs, FncData &_rfd, void */*_pctx*/);
	
	template <typename T>
	static CbkReturnValueE store(Base &_rs, FncData &_rfd, void */*_pctx*/);
	
	template <typename T, class Ser>
	static CbkReturnValueE store(Base &_rs, FncData &_rfd, void */*_pctx*/);
	
	template <typename T, class Ser, class Ctx>
	static CbkReturnValueE store(Base &_rs, FncData &_rfd, void */*_pctx*/);
	
	static CbkReturnValueE storeUtf8(Base &_rs, FncData &_rfd, void */*_pctx*/);
	
	static CbkReturnValueE storeCrossContinue(Base &_rs, FncData &_rfd, void */*_pctx*/);
	template <typename N>
	static CbkReturnValueE storeCross(Base &_rs, FncData &_rfd, void */*_pctx*/);
	
// 	template <typename T, class Ser>
// 	static CbkReturnValueE storeHandle(Base &_rs, FncData &_rfd, void *_pctx){
// 		idbgx(Debug::ser_bin, "store generic non pointer with handle");
// 		Ser		&rs(static_cast<Ser&>(_rs));
// 		if(!rs.cpb) return Success;
// 		T		&rt = *((T*)_rfd.p);
// 		typename Ser::ContextT	&rctx = *reinterpret_cast<typename Ser::ContextT*>(_pctx);
// 		rs.fstk.pop();
// 		serialize(rs, rt, rctx);
// 		return Continue;
// 	}
	
	template <typename T, class Ser, class H>
	static CbkReturnValueE storeHandle(Base &_rb, FncData &_rfd, void *_pctx){
		Ser &rs = static_cast<Ser&>(_rb);
		if(!rs.cpb){
			return Success;
		}
		typename Ser::ContextT	&rctx = *reinterpret_cast<typename Ser::ContextT*>(_pctx);
		H						h;
		T						*pt = reinterpret_cast<T*>(_rfd.p);
		h.afterSerialization(rs, pt, rctx);
		return Success;
	}
	
	template <typename T, class Ser>
	static CbkReturnValueE storeContainer(Base &_rs, FncData &_rfd, void *_pctx){
		idbgx(Debug::ser_bin, "store generic container sizeof(iterator) = "<<sizeof(typename T::iterator)<<" "<<_rfd.n);
		SerializerBase	&rs(static_cast<SerializerBase&>(_rs));
		if(!rs.cpb) return Success;
		T 			*c = reinterpret_cast<T*>(_rfd.p);
		const char	*n = _rfd.n;
		if(c){
			cassert(sizeof(typename T::iterator) <= sizeof(ExtData));
			if(c->size() > rs.lmts.containerlimit){
				rs.err = ERR_CONTAINER_LIMIT;
				return Failure;
			}
			if(c->size() > CRCValue<uint64>::maximum()){
				rs.err = ERR_CONTAINER_MAX_LIMIT;
				return Failure;
			}
			rs.estk.push(ExtData());
			typename T::iterator *pit(new(rs.estk.top().buf) typename T::iterator(c->begin()));
			//typename T::const_iterator &pit = *reinterpret_cast<typename T::const_iterator*>(estk.top().buf);
			*pit = c->begin();
			const CRCValue<uint64>	crcsz(c->size());
			rs.estk.push(ExtData((uint64)crcsz));
			_rfd.f = &SerializerBase::storeContainerContinue<T, Ser>;
			rs.fstk.push(FncData(&Base::popEStack, NULL));
			idbgx(Debug::ser_bin, " sz = "<<rs.estk.top().u64());
			rs.fstk.push(FncData(&SerializerBase::template storeCross<uint64>, &rs.estk.top().u64(), n));
		}else{
			rs.estk.push(ExtData(static_cast<uint64>(-1)));
			rs.fstk.pop();
			rs.fstk.push(FncData(&Base::popEStack, NULL));
			idbgx(Debug::ser_bin, " sz = "<<rs.estk.top().u64());
			rs.fstk.push(FncData(&SerializerBase::template storeCross<uint64>, &rs.estk.top().u64(), n));
		}
		return Continue;
	}
	
	template <typename T, class Ser>
	static CbkReturnValueE storeContainerContinue(Base &_rs, FncData &_rfd, void */*_pctx*/){
		Ser		&rs(static_cast<Ser&>(_rs));
		typename T::iterator &rit = *reinterpret_cast<typename T::iterator*>(rs.estk.top().buf);
		T * c = reinterpret_cast<T*>(_rfd.p);
		if(rs.cpb && rit != c->end()){
			rs.push(*rit, _rfd.n);
			++rit;
			return Continue;
		}
		//TODO:?!how
		//rit.T::~const_iterator();//only call the destructor
		rs.estk.pop();
		return Success;
	}
	
	template <typename T, class Ser>
	static CbkReturnValueE storeArray(Base &_rs, FncData &_rfd, void */*_pctx*/){
		idbgx(Debug::ser_bin, "store generic array "<<_rfd.n);
		SerializerBase	&rs(static_cast<SerializerBase&>(_rs));
		if(!rs.cpb){
			rs.estk.pop();
			return Success;
		}
		
		T			*c = reinterpret_cast<T*>(_rfd.p);
		const char	*n = _rfd.n;
		if(c && rs.estk.top().u64() != static_cast<uint64>(-1)){
			if(rs.estk.top().u64() > rs.lmts.containerlimit){
				rs.err = ERR_ARRAY_LIMIT;
				return Failure;
			}else if(rs.estk.top().u64() <= CRCValue<uint64>::maximum()){
				_rfd.f = &SerializerBase::storeArrayContinue<T, Ser>;
				const CRCValue<uint64>	crcsz(rs.estk.top().u64());
				rs.estk.push(ExtData((uint64)crcsz));
				rs.fstk.push(FncData(&Base::popEStack, NULL));
				idbgx(Debug::ser_bin, "store array size "<<rs.estk.top().u64());
				rs.fstk.push(FncData(&SerializerBase::template storeCross<uint64>, &rs.estk.top().u64(), n));
			}else{
				rs.err = ERR_ARRAY_MAX_LIMIT;
				return Failure;
			}
		}else{
			rs.estk.top().u64() = -1;
			rs.fstk.pop();
			rs.fstk.push(FncData(&Base::popEStack, NULL));
			idbgx(Debug::ser_bin, "store array size "<<rs.estk.top().u64());
			rs.fstk.push(FncData(&SerializerBase::template storeCross<uint64>, &rs.estk.top().u64(), n));
		}
		return Continue;
	}
	
	template <typename T, class Ser>
	static CbkReturnValueE storeArrayContinue(Base &_rs, FncData &_rfd, void */*_pctx*/){
		Ser				&rs(static_cast<Ser&>(_rs));
		T 				*c = reinterpret_cast<T*>(_rfd.p);
		const uint64	&rsz(rs.estk.top().u64());
		int64 			&ri(rs.estk.top().i64_1());
		idbgx(Debug::ser_bin, "store generic array cont "<<_rfd.n<<" rsz = "<<rsz<<" ri = "<<ri);
		
		if(rs.cpb && ri < rsz){
			rs.push(c[ri], _rfd.n);
			++ri;
			return Continue;
		}
		//TODO:?!how
		//rit.T::~const_iterator();//only call the destructor
		rs.estk.pop();
		return Success;
	}
	
	static CbkReturnValueE storeStreamBegin(Base &_rs, FncData &_rfd, void */*_pctx*/);
	static CbkReturnValueE storeStreamCheck(Base &_rs, FncData &_rfd, void */*_pctx*/);
	
	//! Internal callback for storing a stream
	static CbkReturnValueE storeStream(Base &_rs, FncData &_rfd, void */*_pctx*/);
	
	template <class T, class Ser, uint32 I>
	static CbkReturnValueE storeReinit(Base &_rs, FncData &_rfd, void */*_pctx*/){
		Ser				&rs(static_cast<Ser&>(_rs));
		const uint32	val = _rfd.s;

		if(!rs.cpb){
			return Success;
		}

		CbkReturnValueE rv = reinterpret_cast<T*>(_rfd.p)->template serializationReinit<Ser, I>(rs, val);
		if(rv == Failure){
			rs.err = ERR_REINIT;
		}
		return rv;
	}
	
	template <class T, class Ser, uint32 I, class Ctx>
	static CbkReturnValueE storeReinit(Base &_rs, FncData &_rfd, void *_pctx){
		Ser				&rs(static_cast<Ser&>(_rs));
		const uint32	val = _rfd.s;

		if(!rs.cpb){
			return Success;
		}
		Ctx 			&rctx = *reinterpret_cast<Ctx*>(_pctx);
		CbkReturnValueE rv = reinterpret_cast<T*>(_rfd.p)->template serializationReinit<Ser, I>(rs, val, rctx);
		if(rv == Failure){
			rs.err = ERR_REINIT;
		}
		return rv;
	}
	
	static CbkReturnValueE storeReturnError(Base &_rs, FncData &_rfd, void */*_pctx*/){
		SerializerBase		&rs(static_cast<SerializerBase&>(_rs));
		rs.err = _rfd.s;
		return Failure;
	}
	
	void doPushStringLimit();
	void doPushStringLimit(size_t _v);
	void doPushStreamLimit();
	void doPushStreamLimit(uint64 _v);
	void doPushContainerLimit();
	void doPushContainerLimit(size_t _v);
	int run(char *_pb, unsigned _bl, void *_pctx);
public:
	typedef void ContextT;
	
	enum {IsSerializer = true, IsDeserializer = false};
	void clear();
	bool empty()const {return fstk.empty();}
	
	static char* storeValue(char *_pd, const uint8  _val);
	static char* storeValue(char *_pd, const uint16 _val);
	static char* storeValue(char *_pd, const uint32 _val);
	static char* storeValue(char *_pd, const uint64 _val);
protected:
	
	SerializerBase(
		const TypeMapperBase &_rtm
	):pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(ulong));
		typeMapper(_rtm);
	}
	SerializerBase(
		const TypeMapperBase &_rtm,
		Limits const & _rdefaultlmts
	):Base(_rdefaultlmts), pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(ulong));
		typeMapper(_rtm);
	}
	SerializerBase():pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(ulong));
	}
	SerializerBase(
		Limits const & _rdefaultlmts
	):Base(_rdefaultlmts), pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(ulong));
	}
	~SerializerBase();
private:
	template <class T>
	friend struct SerializerPushHelper;
	friend class TypeMapperBase;
	friend class Base;
	char					*pb;
	char					*cpb;
	char					*be;
	std::string				tmpstr;
};
//===============================================================
template <>
CbkReturnValueE SerializerBase::storeBinary<0>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::storeBinary<1>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::storeBinary<2>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::storeBinary<4>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::storeBinary<8>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::store<int8>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::store<uint8>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::store<int16>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::store<uint16>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::store<int32>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::store<uint32>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::store<int64>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::store<uint64>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::store<std::string>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::storeCross<uint8>(Base &_rs, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::storeCross<uint16>(Base &_rs, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::storeCross<uint32>(Base &_rs, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE SerializerBase::storeCross<uint64>(Base &_rs, FncData &_rfd, void */*_pctx*/);

template <typename T>
CbkReturnValueE SerializerBase::store(Base &_rs, FncData &_rfd, void */*_pctx*/){
	//DUMMY - should never get here
	return Failure;
}

template <typename T, class Ser>
CbkReturnValueE SerializerBase::store(Base &_rs, FncData &_rfd, void */*_pctx*/){
	idbgx(Debug::ser_bin, "store generic non pointer");
	Ser &rs(static_cast<Ser&>(_rs));
	if(!rs.cpb) return Success;
	T& rt = *((T*)_rfd.p);
	rs.fstk.pop();
	serialize(rs, rt);
	return Continue;
}

template <typename T, class Ser, class Ctx>
CbkReturnValueE SerializerBase::store(Base &_rs, FncData &_rfd, void *_pctx){
	idbgx(Debug::ser_bin, "store generic non pointer with context");
	Ser		&rs(static_cast<Ser&>(_rs));
	if(!rs.cpb) return Success;
	T		&rt = *((T*)_rfd.p);
	Ctx		&rctx = *reinterpret_cast<Ctx*>(_pctx);
	rs.fstk.pop();
	serialize(rs, rt, rctx);
	return Continue;
}


template <>
struct SerializerPushHelper<int8>{
	void operator()(SerializerBase &_rs, int8 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<int8>, &_rv, _name));
	}
};

template <>
struct SerializerPushHelper<uint8>{
	void operator()(SerializerBase &_rs, uint8 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<uint8>, &_rv, _name));
	}
};


template <>
struct SerializerPushHelper<uint16>{
	void operator()(SerializerBase &_rs, uint16 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<uint16>, &_rv, _name));
	}
};

template <>
struct SerializerPushHelper<int16>{
	void operator()(SerializerBase &_rs, int16 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<int16>, &_rv, _name));
	}
};

template <>
struct SerializerPushHelper<uint32>{
	void operator()(SerializerBase &_rs, uint32 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<uint32>, &_rv, _name));
	}
};

template <>
struct SerializerPushHelper<int32>{
	void operator()(SerializerBase &_rs, int32 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<int32>, &_rv, _name));
	}
};

template <>
struct SerializerPushHelper<uint64>{
	void operator()(SerializerBase &_rs, uint64 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<uint64>, &_rv, _name));
	}
};

template <>
struct SerializerPushHelper<int64>{
	void operator()(SerializerBase &_rs, int64 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<int64>, &_rv, _name));
	}
};

template <>
struct SerializerPushHelper<std::string>{
	void operator()(SerializerBase &_rs, std::string &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<std::string>, &_rv, _name));
	}
};

template <class T>
struct SerializerPushHelper{
	template <class Ser>
	void operator()(Ser &_rs, T &_rv, const char *_name){
		_rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<T, Ser>, &_rv, _name));
	}
	template <class Ser>
	void operator()(Ser &_rs, T &_rv, const char *_name, bool _b){
		typedef typename Ser::ContextT	ContextT;
		_rs.fstk.push(SerializerBase::FncData(&SerializerBase::store<T, Ser, ContextT>, &_rv, _name));
	}
};

//--------------------------------------------------------------
//--------------------------------------------------------------
template <class Ctx = void>
class Serializer;
//--------------------------------------------------------------
template <>
class Serializer<void>: public SerializerBase{
public:
	typedef void 				ContextT;
	typedef Serializer<void>	SerializerT;
	typedef SerializerBase		BaseT;
	
	Serializer(
		const TypeMapperBase &_rtm
	):BaseT(_rtm){
	}
	Serializer(
		const TypeMapperBase &_rtm,
		Limits const & _rdefaultlmts
	):BaseT(_rtm, _rdefaultlmts){
	}
	Serializer(){
	}
	Serializer(
		Limits const & _rdefaultlmts
	):BaseT(_rdefaultlmts){
	}
	
	int run(char *_pb, unsigned _bl){
		return SerializerBase::run(_pb, _bl, NULL);
	}
	
	SerializerT& pushStringLimit(){
		SerializerBase::doPushStringLimit();
		return *this;
	}
	SerializerT& pushStringLimit(size_t _v){
		SerializerBase::doPushStringLimit(_v);
		return *this;
	}
	SerializerT& pushStreamLimit(){
		SerializerBase::doPushStreamLimit();
		return *this;
	}
	SerializerT& pushStreamLimit(uint64 _v){
		SerializerBase::doPushStreamLimit(_v);
		return *this;
	}
	SerializerT& pushContainerLimit(){
		SerializerBase::doPushContainerLimit();
		return *this;
	}
	SerializerT& pushContainerLimit(size_t _v){
		SerializerBase::doPushContainerLimit(_v);
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
	SerializerT& push(T &_t, const char *_name = NULL){
		SerializerPushHelper<T>	sph;
		sph(*this, _t, _name);
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
	SerializerT& push(T* _t, const char *_name = NULL){
		const bool rv = SerializerBase::typeMapper().prepareStorePointer(this, _t, TypeMapperBase::typeName<T>(_t), _name);
		if(!rv){
			SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeReturnError, NULL, _name, SerializerBase::ERR_POINTER_UNKNOWN));
		}
		return *this;
	}
	
	template <typename T, typename TM, typename ID>
	SerializerT& push(
		T* _t, const TM & _rtm,
		const ID &_rid, const char *_name = NULL
	){
		SerializerBase::typeMapper().prepareStorePointer(this, _t, _rtm.realIdentifier(_rid), _name);
		return *this;
	}
	
	//! Schedules a stl style container for serialization
	template <typename T>
	SerializerT& pushContainer(T &_t, const char *_name = NULL){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeContainer<T, SerializerT>, (void*)&_t, _name));
		return *this;
	}
	//! Schedules a pointer to a stl style container for serialization
	template <typename T>
	SerializerT& pushContainer(T *_t, const char *_name = NULL){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeContainer<T, SerializerT>, (void*)_t, _name));
		return *this;
	}
	SerializerT& pushBinary(void *_p, size_t _sz, const char *_name = NULL){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeBinary<0>, _p, _name, _sz));
		return *this;
	}
	template <typename T>
	SerializerT& pushArray(T *_p, const size_t &_rsz, const char *_name = NULL){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeArray<T, SerializerT>, (void*)_p, _name));
		SerializerBase::estk.push(SerializerBase::ExtData((uint64)_rsz, (uint64)0));
		return *this;
	}
	template <typename T>
	SerializerT& pushDynamicArray(
		T* &_rp, const size_t &_rsz, const char *_name = NULL
	){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeArray<T, SerializerT>, (void*)_rp, _name));
		SerializerBase::estk.push(SerializerBase::ExtData((uint64)_rsz, (uint64)0));
		return *this;
	}
	SerializerT& pushUtf8(
		const std::string& _str, const char *_name = NULL
	){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeUtf8, const_cast<char*>(_str.c_str()), _name, _str.length() + 1));
		return *this;
	}
	template <class T, uint32 I>
	SerializerT& pushReinit(
		T *_pt, const uint64 &_rval = 0, const char *_name = NULL
	){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeReinit<T, SerializerT, I>, _pt, _name, _rval));
		return *this;
	}
	SerializerT& pushStream(
		std::istream *_ps, const char *_name = NULL
	){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStream, _ps, _name, -1ULL));
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStreamBegin, _ps, _name, -1ULL));
		return *this;
	}
	SerializerT& pushStream(
		std::istream *_ps,
		const uint64 &_rfrom,
		const uint64 &_rlen,
		const char *_name = NULL
	){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStream, _ps, _name, _rlen));
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStreamBegin, _ps, _name, _rfrom));
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStreamCheck, _ps, _name, _rlen));
		return *this;
	}
	SerializerT& pushStream(
		std::ostream *_ps, const char *_name = NULL
	);
	SerializerT& pushStream(
		std::ostream *_ps,
		const uint64 &_rfrom,
		const uint64 &_rlen,
		const char *_name = NULL
	);
	template <typename T, class H>
	SerializerT& pushHandlePointer(T *_pt, const char *_name){
		this->Base::fstk.push(Base::FncData(&SerializerBase::storeHandle<T, SerializerT, H>, _pt, _name));
		return *this;
	}
	
	SerializerT& pushCross(const uint8 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint8>, const_cast<uint8*>(&_rv), _name));
		return *this;
	}
	SerializerT& pushCross(const uint16 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint16>, const_cast<uint16*>(&_rv), _name));
		return *this;
	}
	SerializerT& pushCross(const uint32 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint32>, const_cast<uint32*>(&_rv), _name));
		return *this;
	}
	SerializerT& pushCross(const uint64 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint64>, const_cast<uint64*>(&_rv), _name));
		return *this;
	}
};
//--------------------------------------------------------------
template <class Ctx>
class Serializer: public SerializerBase{
public:
	typedef Ctx 				ContextT;
	typedef Serializer<Ctx>		SerializerT;
	typedef SerializerBase		BaseT;
	Serializer(
		const TypeMapperBase &_rtm
	):BaseT(_rtm){
	}
	Serializer(
		const TypeMapperBase &_rtm,
		Limits const & _rdefaultlmts
	):BaseT(_rtm, _rdefaultlmts){
	}
	Serializer(){
	}
	Serializer(
		Limits const & _rdefaultlmts
	):BaseT(_rdefaultlmts){
	}
	
	int run(char *_pb, unsigned _bl, Ctx &_rctx){
		const void *pctx = &_rctx;
		return SerializerBase::run(_pb, _bl, const_cast<void *>(pctx));
	}
	
	SerializerT& pushStringLimit(){
		SerializerBase::doPushStringLimit();
		return *this;
	}
	SerializerT& pushStringLimit(size_t _v){
		SerializerBase::doPushStringLimit(_v);
		return *this;
	}
	SerializerT& pushStreamLimit(){
		SerializerBase::doPushStreamLimit();
		return *this;
	}
	SerializerT& pushStreamLimit(uint64 _v){
		SerializerBase::doPushStreamLimit(_v);
		return *this;
	}
	SerializerT& pushContainerLimit(){
		SerializerBase::doPushContainerLimit();
		return *this;
	}
	SerializerT& pushContainerLimit(size_t _v){
		SerializerBase::doPushContainerLimit(_v);
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
	SerializerT& push(T &_t, const char *_name = NULL){
		//fstk.push(FncData(&SerializerBase::template store<T, SerializerT>, (void*)&_t, _name));
		SerializerPushHelper<T>	sph;
		sph(*this, _t, _name, true);
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
	SerializerT& push(T* _t, const char *_name = NULL){
		const bool rv = SerializerBase::typeMapper().prepareStorePointer(this, _t, TypeMapperBase::typeName<T>(_t), _name);
		if(!rv){
			SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeReturnError, NULL, _name, SerializerBase::ERR_POINTER_UNKNOWN));
		}
		return *this;
	}
	
	template <typename T, typename TM, typename ID>
	SerializerT& push(
		T* _t, const TM & _rtm,
		const ID &_rid, const char *_name = NULL
	){
		SerializerBase::typeMapper().prepareStorePointer(this, _t, _rtm.realIdentifier(_rid), _name);
		return *this;
	}
	
	//! Schedules a stl style container for serialization
	template <typename T>
	SerializerT& pushContainer(T &_t, const char *_name = NULL){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeContainer<T, SerializerT>, (void*)&_t, _name));
		return *this;
	}
	//! Schedules a pointer to a stl style container for serialization
	template <typename T>
	SerializerT& pushContainer(T *_t, const char *_name = NULL){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeContainer<T, SerializerT>, (void*)_t, _name));
		return *this;
	}
	SerializerT& pushBinary(void *_p, size_t _sz, const char *_name = NULL){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeBinary<0>, _p, _name, _sz));
		return *this;
	}
	template <typename T, typename ST>
	SerializerT& pushArray(T *_p, const ST &_rsz, const char *_name = NULL){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeArray<T, SerializerT>, (void*)_p, _name));
		SerializerBase::estk.push(SerializerBase::ExtData((uint64)_rsz, (uint64)0));
		return *this;
	}
	template <typename T, typename ST>
	SerializerT& pushDynamicArray(
		T* &_rp, const ST &_rsz, const char *_name = NULL
	){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeArray<T, SerializerT>, (void*)_rp, _name));
		SerializerBase::estk.push(SerializerBase::ExtData((uint64)_rsz, (uint64)0));
		return *this;
	}
	SerializerT& pushUtf8(
		const std::string& _str, const char *_name = NULL
	){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeUtf8, const_cast<char*>(_str.c_str()), _name, _str.length() + 1));
		return *this;
	}
	template <class T, uint32 I>
	SerializerT& pushReinit(
		T *_pt, const uint64 &_rval = 0, const char *_name = NULL
	){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template storeReinit<T, SerializerT, I, Ctx>, _pt, _name, _rval));
		return *this;
	}
	SerializerT& pushStream(
		std::istream *_ps, const char *_name = NULL
	){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStream, _ps, _name, -1ULL));
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStreamBegin, _ps, _name, -1ULL));
		return *this;
	}
	SerializerT& pushStream(
		std::istream *_ps,
		const uint64 &_rfrom,
		const uint64 &_rlen,
		const char *_name = NULL
	){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStream, _ps, _name, _rlen));
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStreamBegin, _ps, _name, _rfrom));
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::storeStreamCheck, _ps, _name, _rlen));
		return *this;
	}
	SerializerT& pushStream(
		std::ostream *_ps, const char *_name = NULL
	);
	SerializerT& pushStream(
		std::ostream *_ps,
		const uint64 &_rfrom,
		const uint64 &_rlen,
		const char *_name = NULL
	);
	template <class H, class T>
	SerializerT& pushHandle(T *_pt, const char *_name = NULL){
		SerializerBase::fstk.push(SerializerBase::FncData(&SerializerBase::template handle<T, SerializerT, H, Ctx>, _pt, _name));
		return *this;
	}
	
	template <typename T, class H>
	SerializerT& pushHandlePointer(T *_pt, const char *_name){
		this->Base::fstk.push(Base::FncData(&SerializerBase::storeHandle<T, SerializerT, H>, _pt, _name));
		return *this;
	}
	SerializerT& pushCross(const uint8 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint8>, &_rv, _name));
		return *this;
	}
	SerializerT& pushCross(const uint16 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint16>, &_rv, _name));
		return *this;
	}
	SerializerT& pushCross(const uint32 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint32>, &_rv, _name));
		return *this;
	}
	SerializerT& pushCross(const uint64 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&SerializerBase::storeCross<uint64>, &_rv, _name));
		return *this;
	}
};

//===============================================================
template <class T>
struct DeserializerPushHelper;
//! A reentrant binary deserializer
/*!
	See serialization::bin::Base for details
*/
class DeserializerBase: public Base{
protected:
	template <typename T>
	static void dynamicPointerInit(void *_pptr, void *_pt){
		DynamicPointer<T>	&dp = *reinterpret_cast<DynamicPointer<T>*>(_pptr);
		T					*pt = reinterpret_cast<T*>(_pt);
		dp = pt;
	}
	static CbkReturnValueE loadTypeIdDone(Base& _rd, FncData &_rfd, void */*_pctx*/);
	static CbkReturnValueE loadTypeId(Base& _rd, FncData &_rfd, void */*_pctx*/);
	
	template <typename T>
	static CbkReturnValueE loadDynamicTypeId(Base& _rd, FncData &_rfd, void */*_pctx*/){
		DeserializerBase &rd(static_cast<DeserializerBase&>(_rd));
	
		if(!rd.cpb) return Success;
	
		rd.typeMapper().prepareParsePointerId(&rd, rd.tmpstr, _rfd.n);
		_rfd.f = &loadDynamicTypeIdDone<T>;
		return Continue;
	}
	
	template <typename T>
	static CbkReturnValueE loadDynamicTypeIdDone(Base& _rd, FncData &_rfd, void *_pctx){
		DeserializerBase	&rd(static_cast<DeserializerBase&>(_rd));
		void				*p = _rfd.p;
		const char			*n = _rfd.n;
		
		if(!rd.cpb) return Success;
		
		rd.fstk.pop();
		
		if(rd.typeMapper().prepareParsePointer(&rd, rd.tmpstr, p, n, _pctx, &dynamicPointerInit<T>)){
			return Continue;
		}else{
			idbgx(Debug::ser_bin, "error");
			rd.err = ERR_POINTER_UNKNOWN;
			return Failure;
		}
	}
	
	template <typename T>
	static CbkReturnValueE loadCrossDone(Base& _rd, FncData &_rfd, void */*_pctx*/){
		DeserializerBase	&rd(static_cast<DeserializerBase&>(_rd));
		
		if(!rd.cpb){
			rd.estk.pop();
			return Success;
		}
		T	&v = *reinterpret_cast<T*>(_rfd.p);
		v = static_cast<T>(rd.estk.top().u64());
		rd.estk.pop();
		return Success;
	}
	
	static CbkReturnValueE loadCrossContinue(Base& _rd, FncData &_rfd, void */*_pctx*/);
	template <typename T>
	static CbkReturnValueE loadCross(Base& _rd, FncData &_rfd, void */*_pctx*/);
	
	template <typename T>
	static CbkReturnValueE load(Base& _rd, FncData &_rfd, void */*_pctx*/);
	
	template <typename T, class Des>
	static CbkReturnValueE load(Base& _rd, FncData &_rfd, void */*_pctx*/);
	
	template <typename T, class Des, class Ctx>
	static CbkReturnValueE load(Base& _rd, FncData &_rfd, void */*_pctx*/);
	
	template <uint S>
	static CbkReturnValueE loadBinary(Base &_rb, FncData &_rfd, void */*_pctx*/);
	
	static CbkReturnValueE loadBinaryString(Base &_rb, FncData &_rfd, void */*_pctx*/);
	static CbkReturnValueE loadBinaryStringCheck(Base &_rb, FncData &_rfd, void */*_pctx*/);
	static CbkReturnValueE loadUtf8(Base &_rb, FncData &_rfd, void */*_pctx*/);
	
	template <typename T, class Des, class H>
	static CbkReturnValueE loadHandle(Base &_rb, FncData &_rfd, void *_pctx){
		Des &rd = static_cast<Des&>(_rb);
		if(!rd.cpb){
			return Success;
		}
		typename Des::ContextT	&rctx = *reinterpret_cast<typename Des::ContextT*>(_pctx);
		H						h;
		T						*pt = reinterpret_cast<T*>(_rfd.p);
		h.afterSerialization(rd, pt, rctx);
		return Success;
	}
	
	template <typename T, class Des>
	static CbkReturnValueE loadContainer(Base &_rb, FncData &_rfd, void */*_pctx*/){
		idbgx(Debug::ser_bin, "load generic non pointer container "<<_rfd.n);
		DeserializerBase &rd = static_cast<DeserializerBase&>(_rb);
		if(!rd.cpb) return Success;
		_rfd.f = &DeserializerBase::loadContainerBegin<T, Des>;
		rd.estk.push(ExtData((uint64)0));
		rd.fstk.push(FncData(&DeserializerBase::loadCross<uint64>, &rd.estk.top().u64()));
		return Continue;
	}
	template <typename T, class Des>
	static CbkReturnValueE loadContainerBegin(Base &_rb, FncData &_rfd, void */*_pctx*/){
		DeserializerBase	&rd = static_cast<DeserializerBase&>(_rb);
		
		if(!rd.cpb){
			rd.estk.pop();
			return Success;
		}
		{
			const uint64	i = rd.estk.top().u64();
			idbgx(Debug::ser_bin, " sz = "<<i);
			if(i != static_cast<uint64>(-1)){
				const CRCValue<uint64> crcsz(CRCValue<uint64>::check_and_create(i));
				if(crcsz.ok()){
					rd.estk.top().u64() = crcsz.value();
				}else{
					rd.err = ERR_CONTAINER_MAX_LIMIT;
					return Failure;
				}
			}
		}
		const uint64 i(rd.estk.top().u64());
		vdbgx(Debug::ser_bin, "i = "<<i);
		
		if(
			i != static_cast<uint64>(-1) && 
			i > rd.lmts.containerlimit
		){
			idbgx(Debug::ser_bin, "error");
			rd.err = ERR_CONTAINER_LIMIT;
			return Failure;
		}
		
		if(i == static_cast<uint64>(-1)){
			cassert(!_rfd.s);
			T **c = reinterpret_cast<T**>(_rfd.p);
			*c = NULL;
			rd.estk.pop();
			return Success;
		}else if(!_rfd.s){
			T **c = reinterpret_cast<T**>(_rfd.p);
			vdbgx(Debug::ser_bin, "");
			*c = new T;
			_rfd.p = *c;
		}
		if(i){
			_rfd.f = &DeserializerBase::loadContainerContinue<T, Des>;
			_rfd.s = 0;//(uint32)i;
			return Continue;
		}
		rd.estk.pop();
		return Success;
	}
	template <typename T, class Des>
	static CbkReturnValueE loadContainerContinue(Base &_rb, FncData &_rfd, void */*_pctx*/){
		Des			&rd(static_cast<Des&>(_rb));
		uint64		&ri = rd.estk.top().u64();
		if(rd.cpb && ri){
			T *c = reinterpret_cast<T*>(_rfd.p);
			c->push_back(typename T::value_type());
			rd.push(c->back());
			--ri;
			return Continue;	
		}
		rd.estk.pop();
		return Success;
	}
	
	template <typename T, class Des>
	static CbkReturnValueE loadArray(Base &_rb, FncData &_rfd, void */*_pctx*/){
		idbgx(Debug::ser_bin, "load generic array");
		DeserializerBase &rd(static_cast<DeserializerBase&>(_rb));
		if(!rd.cpb){
			rd.estk.pop();
			return Success;
		}
		{
			const uint64	&rsz(rd.estk.top().u64());
			idbgx(Debug::ser_bin, "size "<<rsz);
			if(rsz != static_cast<uint64>(-1)){
				const CRCValue<uint64> crcsz(CRCValue<uint64>::check_and_create(rsz));
				if(crcsz.ok()){
					rd.estk.top().u64() = crcsz.value();
				}else{
					rd.err = ERR_ARRAY_MAX_LIMIT;
					return Failure;
				}
			}
		}
		const uint64	&rsz(rd.estk.top().u64());
		size_t			&rextsz(*reinterpret_cast<size_t*>(rd.estk.top().pv_2()));
		idbgx(Debug::ser_bin, "size "<<rsz);
		if(rsz != static_cast<uint64>(-1) && rsz > rd.lmts.containerlimit){
			idbgx(Debug::ser_bin, "error");
			rd.err = ERR_ARRAY_LIMIT;
			return Failure;
		}
		
		if(rsz == static_cast<uint64>(-1)){
			cassert(!_rfd.s);
			T **c = reinterpret_cast<T**>(_rfd.p);
			*c = NULL;
			rd.estk.pop();
			rextsz = 0;
			return Success;
		}else if(!_rfd.s){
			T **c = reinterpret_cast<T**>(_rfd.p);
			vdbgx(Debug::ser_bin, "");
			*c = new T[rsz];
			_rfd.p = *c;
		}
		rextsz = rsz;
		_rfd.f = &DeserializerBase::loadArrayContinue<T, Des>;
		return Continue;
	}
	template <typename T, class Des>
	static CbkReturnValueE loadArrayContinue(Base &_rb, FncData &_rfd, void */*_pctx*/){
		Des					&rd(static_cast<Des&>(_rb));
		const uint64		&rsz = rd.estk.top().u64();
		uint64				&ri = rd.estk.top().u64_1();
		
		idbgx(Debug::ser_bin, "load generic array continue "<<_rfd.n<<" idx = "<<ri<<" sz = "<<rsz);
		
		if(rd.cpb && ri < rsz){
			T *c = reinterpret_cast<T*>(_rfd.p);
			rd.push(c[ri]);
			++ri;
			return Continue;	
		}
		rd.estk.pop();
		return Success;
	}
	
	//! Internal callback for parsing a stream
	static CbkReturnValueE loadStream(Base &_rb, FncData &_rfd, void */*_pctx*/);
	static CbkReturnValueE loadStreamBegin(Base &_rb, FncData &_rfd, void */*_pctx*/);
	static CbkReturnValueE loadStreamCheck(Base &_rb, FncData &_rfd, void */*_pctx*/);
	//! Internal callback for parsign a dummy stream
	/*!
		This is internally called when a deserialization destionation
		ostream could not be optained, or there was an error while
		writing to destination stream.
	*/
	static CbkReturnValueE loadDummyStream(Base &_rb, FncData &_rfd, void */*_pctx*/);
	
	template <class T, class Des, uint32 I>
	static CbkReturnValueE loadReinit(Base &_rb, FncData &_rfd, void */*_pctx*/){
		Des				&rd(static_cast<Des&>(_rb));
		const uint32	val = _rfd.s;
		
		if(!rd.cpb){
			return Success;
		}
		
		CbkReturnValueE rv = reinterpret_cast<T*>(_rfd.p)->template serializationReinit<Des, I>(rd, val);
		if(rv == Failure){
			rd.err = ERR_REINIT;
		}
		return rv;
	}
	
	template <class T, class Des, uint32 I, class Ctx>
	static CbkReturnValueE loadReinit(Base &_rb, FncData &_rfd, void *_pctx){
		Des				&rd(static_cast<Des&>(_rb));
		const uint32	val = _rfd.s;
		
		if(!rd.cpb){
			return Success;
		}
		Ctx 			&rctx = *reinterpret_cast<Ctx*>(_pctx);
		CbkReturnValueE rv = reinterpret_cast<T*>(_rfd.p)->template serializationReinit<Des, I>(rd, val, rctx);
		if(rv == Failure){
			rd.err = ERR_REINIT;
		}
		return rv;
	}
	
	void doPushStringLimit();
	void doPushStringLimit(size_t _v);
	void doPushStreamLimit();
	void doPushStreamLimit(uint64 _v);
	void doPushContainerLimit();
	void doPushContainerLimit(size_t _v);
	int run(const char *_pb, unsigned _bl, void *_pctx);
public:
	typedef void ContextT;
	
	enum {IsSerializer = false, IsDeserializer = true};
	static const char* loadValue(const char *_ps, uint8  &_val);
	static const char* loadValue(const char *_ps, uint16 &_val);
	static const char* loadValue(const char *_ps, uint32 &_val);
	static const char* loadValue(const char *_ps, uint64 &_val);
	DeserializerBase(
		const TypeMapperBase &_rtm
	):pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(uint32));
		typeMapper(_rtm);
	}
	DeserializerBase(
		const TypeMapperBase &_rtm,
		Limits const & _rdefaultlmts
	):Base(_rdefaultlmts), pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(uint32));
		typeMapper(_rtm);
	}
	DeserializerBase():pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(uint32));
	}
	DeserializerBase(
		Limits const & _rdefaultlmts
	):Base(_rdefaultlmts), pb(NULL), cpb(NULL), be(NULL){
		tmpstr.reserve(sizeof(uint32));
	}
	~DeserializerBase();
	
	void clear();
	bool empty()const {return fstk.empty();}
private:
	template <class T>
	friend struct DeserializerPushHelper;
	friend class Base;
	const char				*pb;
	const char				*cpb;
	const char				*be;
	std::string				tmpstr;
};

template <>
CbkReturnValueE DeserializerBase::loadBinary<0>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE DeserializerBase::loadBinary<1>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE DeserializerBase::loadBinary<2>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE DeserializerBase::loadBinary<4>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE DeserializerBase::loadBinary<8>(Base &_rb, FncData &_rfd, void */*_pctx*/);

template <>
CbkReturnValueE DeserializerBase::load<int8>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE DeserializerBase::load<uint8>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE DeserializerBase::load<int16>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE DeserializerBase::load<uint16>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE DeserializerBase::load<int32>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE DeserializerBase::load<uint32>(Base &_rb, FncData &_rfd, void */*_pctx*/);

template <>
CbkReturnValueE DeserializerBase::load<int64>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE DeserializerBase::load<uint64>(Base &_rb, FncData &_rfd, void */*_pctx*/);
template <>
CbkReturnValueE DeserializerBase::load<std::string>(Base &_rb, FncData &_rfd, void */*_pctx*/);

template <>
/*static*/ CbkReturnValueE DeserializerBase::loadCross<uint8>(Base& _rd, FncData &_rfd, void */*_pctx*/);
template <>
/*static*/ CbkReturnValueE DeserializerBase::loadCross<uint16>(Base& _rd, FncData &_rfd, void */*_pctx*/);
template <>
/*static*/ CbkReturnValueE DeserializerBase::loadCross<uint32>(Base& _rd, FncData &_rfd, void */*_pctx*/);
template <>
/*static*/ CbkReturnValueE DeserializerBase::loadCross<uint64>(Base& _rd, FncData &_rfd, void */*_pctx*/);

template <typename T>
CbkReturnValueE DeserializerBase::load(Base& _rd, FncData &_rfd, void */*_pctx*/){
	//should never get here
	return Failure;
}
template <typename T, class Des>
CbkReturnValueE DeserializerBase::load(Base& _rd, FncData &_rfd, void *_pctx){
	idbgx(Debug::ser_bin, "load generic non pointer");
	Des &rd(static_cast<Des&>(_rd));
	if(!rd.cpb) return Success;
	rd.fstk.pop();
	serialize(rd, *reinterpret_cast<T*>(_rfd.p));
	return Continue;
}

template <typename T, class Des, class Ctx>
CbkReturnValueE DeserializerBase::load(Base& _rd, FncData &_rfd, void *_pctx){
	idbgx(Debug::ser_bin, "load generic non pointer");
	Des &rd(static_cast<Des&>(_rd));
	if(!rd.cpb) return Success;
	rd.fstk.pop();
	Ctx &rctx = *reinterpret_cast<Ctx*>(_pctx);
	//*reinterpret_cast<T*>(_rfd.p) & rd;
	serialize(rd, *reinterpret_cast<T*>(_rfd.p), rctx);
	return Continue;
}


template <>
struct DeserializerPushHelper<int8>{
	void operator()(DeserializerBase &_rs, int8 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<int8>, &_rv, _name));
	}
};

template <>
struct DeserializerPushHelper<uint8>{
	void operator()(DeserializerBase &_rs, uint8 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<uint8>, &_rv, _name));
	}
};


template <>
struct DeserializerPushHelper<uint16>{
	void operator()(DeserializerBase &_rs, uint16 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<uint16>, &_rv, _name));
	}
};

template <>
struct DeserializerPushHelper<int16>{
	void operator()(DeserializerBase &_rs, int16 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<int16>, &_rv, _name));
	}
};

template <>
struct DeserializerPushHelper<uint32>{
	void operator()(DeserializerBase &_rs, uint32 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<uint32>, &_rv, _name));
	}
};

template <>
struct DeserializerPushHelper<int32>{
	void operator()(DeserializerBase &_rs, int32 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<int32>, &_rv, _name));
	}
};

template <>
struct DeserializerPushHelper<uint64>{
	void operator()(DeserializerBase &_rs, uint64 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<uint64>, &_rv, _name));
	}
};

template <>
struct DeserializerPushHelper<int64>{
	void operator()(DeserializerBase &_rs, int64 &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<int64>, &_rv, _name));
	}
};

template <>
struct DeserializerPushHelper<std::string>{
	void operator()(DeserializerBase &_rs, std::string &_rv, const char *_name, bool _b = false){
		_rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<std::string>, &_rv, _name));
	}
};

template <class T>
struct DeserializerPushHelper{
	template <class Des>
	void operator()(Des &_rs, T &_rv, const char *_name){
		_rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<T, Des>, &_rv, _name));
	}
	template <class Des>
	void operator()(Des &_rs, T &_rv, const char *_name, bool _b){
		typedef typename Des::ContextT	ContextT;
		_rs.fstk.push(DeserializerBase::FncData(&DeserializerBase::load<T, Des, ContextT>, &_rv, _name));
	}
};



//--------------------------------------------------------------
//--------------------------------------------------------------
template <class Ctx = void>
class Deserializer;

template <>
class Deserializer<void>: public DeserializerBase{
public:
	typedef void 					ContextT;
	typedef Deserializer<void>		DeserializerT;
	typedef DeserializerBase		BaseT;
	
	Deserializer(
		const TypeMapperBase &_rtm
	):BaseT(_rtm){
	}
	Deserializer(
		const TypeMapperBase &_rtm,
		Limits const & _rdefaultlmts
	):BaseT(_rtm, _rdefaultlmts){
	}
	Deserializer(){
	}
	Deserializer(
		Limits const & _rdefaultlmts
	):BaseT(_rdefaultlmts){
	}
	
	int run(const char *_pb, unsigned _bl){
		return DeserializerBase::run(_pb, _bl, NULL);
	}
	
	Deserializer& pushStringLimit(){
		this->doPushStringLimit();
		return *this;
	}
	Deserializer& pushStringLimit(size_t _v){
		this->doPushStringLimit(_v);
		return *this;
	}
	Deserializer& pushStreamLimit(){
		this->doPushStreamLimit();
		return *this;
	}
	Deserializer& pushStreamLimit(uint64 _v){
		this->doPushStreamLimit(_v);
		return *this;
	}
	Deserializer& pushContainerLimit(){
		this->doPushContainerLimit();
		return *this;
	}
	Deserializer& pushContainerLimit(size_t _v){
		this->doPushContainerLimit(_v);
		return *this;
	}
	
	template <typename T>
	Deserializer& push(T &_t, const char *_name = NULL){
		DeserializerPushHelper<T>	ph;
		ph(*this, _t, _name);
		return *this;
	}
	
	template <typename T>
	Deserializer& push(T* &_t, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&Deserializer::loadTypeId, &_t, _name));
		return *this;
	}
	template <class T, uint32 I>
	Deserializer& pushReinit(
		T *_pt, const uint64 &_rval = 0, const char *_name = NULL
	){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadReinit<T, DeserializerT, I>, _pt, _name, _rval));
		return *this;
	}
	//! Schedules a std (style) container for deserialization
	template <typename T>
	Deserializer& pushContainer(T &_t, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadContainer<T, DeserializerT>, (void*)&_t, _name));
		return *this;
	}
	//! Schedules a std (style) container for deserialization
	template <typename T>
	Deserializer& pushContainer(T* &_t, const char *_name = NULL){
		cassert(!_t);//the pointer must be null!!
		this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadContainer<T, DeserializerT>, (void*)&_t, _name, 0));
		return *this;
	}
	Deserializer& pushBinary(void *_p, size_t _sz, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadBinary<0>, _p, _name, _sz));
		return *this;
	}
	
	template <typename T>
	Deserializer& pushArray(T* _p, size_t &_rsz, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadArray<T, DeserializerT>, (void*)_p, _name));
		this->Base::estk.push(Base::ExtData((uint64)0, (uint64)0, (void*)&_rsz));
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint64>, &this->Base::estk.top().u64()));
		return *this;
	}
	template <typename T>
	Deserializer& pushDynamicArray(T* &_p, size_t &_rsz, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadArray<T, DeserializerT>, (void*)&_p, _name, 0));
		this->Base::estk.push(Base::ExtData((uint64)0, (uint64)0, (void*)&_rsz));
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint64>, &this->Base::estk.top().u64()));
		return *this;
	}
	Deserializer& pushUtf8(
		std::string& _str, const char *_name = NULL
	){
		_str.clear();
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadUtf8, &_str, _name, 0));
		return *this;
	}
	
	Deserializer& pushStream(
		std::ostream *_ps, const char *_name = NULL
	){
		if(_ps){
			this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStream, _ps, _name, -1ULL));
		}else{
			this->Base::fstk.push(Base::FncData(&DeserializerBase::loadDummyStream, _ps, _name, -1ULL));
		}
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStreamBegin, _ps, _name, 0));
		return *this;
	}
	Deserializer& pushStream(
		std::ostream *_ps,
		const uint64 &_rat,
		const uint64 &_rlen,
		const char *_name = NULL
	){
		if(_ps){
			this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStream, _ps, _name, _rlen));
		}else{
			this->Base::fstk.push(Base::FncData(&DeserializerBase::loadDummyStream, _ps, _name, _rlen));
		}
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStreamBegin, _ps, _name, _rat));
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStreamCheck, _ps, _name, _rlen));
		return *this;
	}
	Deserializer& pushStream(
		std::istream *_ps, const char *_name = NULL
	);
	Deserializer& pushStream(
		std::istream *_ps,
		const uint64 &_rat,
		const uint64 &,
		const char *_name = NULL
	);
	template <typename T, class H>
	Deserializer& pushHandlePointer(T *_pt, const char *_name){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadHandle<T, DeserializerT, H>, _pt, _name));
		return *this;
	}
	Deserializer& pushCross(uint8 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint8>, &_rv, _name));
		return *this;
	}
	Deserializer& pushCross(uint16 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint16>, &_rv, _name));
		return *this;
	}
	Deserializer& pushCross(uint32 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint32>, &_rv, _name));
		return *this;
	}
	Deserializer& pushCross(uint64 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint64>, &_rv, _name));
		return *this;
	}
};

//--------------------------------------------------------------
//--------------------------------------------------------------

template <class Ctx>
class Deserializer: public DeserializerBase{
public:
	typedef Ctx 					ContextT;
	typedef Deserializer<Ctx>		DeserializerT;
	typedef DeserializerBase		BaseT;
	Deserializer(
		const TypeMapperBase &_rtm
	):BaseT(_rtm){
	}
	Deserializer(
		const TypeMapperBase &_rtm,
		Limits const & _rdefaultlmts
	):BaseT(_rtm, _rdefaultlmts){
	}
	Deserializer(){
	}
	Deserializer(
		Limits const & _rdefaultlmts
	):BaseT(_rdefaultlmts){
	}
	
	int run(const char *_pb, unsigned _bl, Ctx &_rctx){
		const void *pctx = &_rctx;
		return DeserializerBase::run(_pb, _bl, const_cast<void*>(pctx));
	}
	
	Deserializer& pushStringLimit(){
		this->doPushStringLimit();
		return *this;
	}
	Deserializer& pushStringLimit(size_t _v){
		this->doPushStringLimit(_v);
		return *this;
	}
	Deserializer& pushStreamLimit(){
		this->doPushStreamLimit();
		return *this;
	}
	Deserializer& pushStreamLimit(uint64 _v){
		this->doPushStreamLimit(_v);
		return *this;
	}
	Deserializer& pushContainerLimit(){
		this->doPushContainerLimit();
		return *this;
	}
	Deserializer& pushContainerLimit(size_t _v){
		this->doPushContainerLimit(_v);
		return *this;
	}
	
	template <typename T>
	Deserializer& push(T &_t, const char *_name = NULL){
		DeserializerPushHelper<T>	ph;
		ph(*this, _t, _name, true);
		return *this;
	}
	
	template <typename T>
	Deserializer& push(T* &_t, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&Deserializer::loadTypeId, &_t, _name));
		return *this;
	}
	
	template <typename T>
	Deserializer& push(DynamicPointer<T> &_rdp, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&Deserializer::template loadDynamicTypeId<T>, &_rdp, _name));
		return *this;
	}
	
	template <class T, uint32 I>
	Deserializer& pushReinit(
		T *_pt, const uint64 &_rval = 0, const char *_name = NULL
	){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadReinit<T, DeserializerT, I, ContextT>, _pt, _name, _rval));
		return *this;
	}
	//! Schedules a std (style) container for deserialization
	template <typename T>
	Deserializer& pushContainer(T &_t, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadContainer<T, DeserializerT>, (void*)&_t, _name));
		return *this;
	}
	//! Schedules a std (style) container for deserialization
	template <typename T>
	Deserializer& pushContainer(T* &_t, const char *_name = NULL){
		cassert(!_t);//the pointer must be null!!
		this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadContainer<T, DeserializerT>, (void*)&_t, _name, 0));
		return *this;
	}
	Deserializer& pushBinary(void *_p, size_t _sz, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadBinary<0>, _p, _name, _sz));
		return *this;
	}
	
	template <typename T>
	Deserializer& pushArray(T* _p, size_t &_rsz, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadArray<T, DeserializerT>, (void*)_p, _name));
		this->Base::estk.push(Base::ExtData((uint64)0, (uint64)0, (void*)&_rsz));
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint64>, &this->Base::estk.top().u64()));
		return *this;
	}
	template <typename T>
	Deserializer& pushDynamicArray(T* &_p, size_t &_rsz, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::template loadArray<T, DeserializerT>, (void*)&_p, _name, 0));
		this->Base::estk.push(Base::ExtData((uint64)0, (uint64)0, (void*)&_rsz));
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint64>, &this->Base::estk.top().u64()));
		return *this;
	}
	Deserializer& pushUtf8(
		std::string& _str, const char *_name = NULL
	){
		_str.clear();
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadUtf8, &_str, _name, 0));
		return *this;
	}
	
	Deserializer& pushStream(
		std::ostream *_ps, const char *_name = NULL
	){
		if(_ps){
			this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStream, _ps, _name, -1ULL));
		}else{
			this->Base::fstk.push(Base::FncData(&DeserializerBase::loadDummyStream, _ps, _name, -1ULL));
		}
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStreamBegin, _ps, _name, 0));
		return *this;
	}
	Deserializer& pushStream(
		std::ostream *_ps,
		const uint64 &_rat,
		const uint64 &_rlen,
		const char *_name = NULL
	){
		if(_ps){
			this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStream, _ps, _name, _rlen));
		}else{
			this->Base::fstk.push(Base::FncData(&DeserializerBase::loadDummyStream, _ps, _name, _rlen));
		}
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStreamBegin, _ps, _name, _rat));
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadStreamCheck, _ps, _name, _rlen));
		return *this;
	}
	Deserializer& pushStream(
		std::istream *_ps, const char *_name = NULL
	);
	Deserializer& pushStream(
		std::istream *_ps,
		const uint64 &_rat,
		const uint64 &,
		const char *_name = NULL
	);
	template <typename T, class H>
	Deserializer& pushHandlePointer(T *_pt, const char *_name){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadHandle<T, DeserializerT, H>, _pt, _name));
		return *this;
	}
	template <class H, class T>
	Deserializer& pushHandle(T *_pt, const char *_name = NULL){
		DeserializerBase::fstk.push(DeserializerBase::FncData(&DeserializerBase::template handle<T, DeserializerT, H, Ctx>, _pt, _name));
		return *this;
	}
	Deserializer& pushCross(uint8 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint8>, &_rv, _name));
		return *this;
	}
	Deserializer& pushCross(uint16 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint16>, &_rv, _name));
		return *this;
	}
	Deserializer& pushCross(uint32 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint32>, &_rv, _name));
		return *this;
	}
	Deserializer& pushCross(uint64 &_rv, const char *_name = NULL){
		this->Base::fstk.push(Base::FncData(&DeserializerBase::loadCross<uint64>, &_rv, _name));
		return *this;
	}
};

}//namespace binary
}//namespace serialization
}//namespace solid

#endif
