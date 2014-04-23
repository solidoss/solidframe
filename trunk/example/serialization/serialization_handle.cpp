// serialization_handle.cpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <iostream>
#include <stack>
#include <vector>
#include <deque>
#include <map>
#include <list>
//#undef UDEBUG
#include "system/thread.hpp"
#include "system/debug.hpp"
#include "serialization/idtypemapper.hpp"
#include "serialization/binary.hpp"
#include "system/socketaddress.hpp"

using namespace std;
using namespace solid;

struct Context{
	Context(int _v = 10):value(_v){}
	int value;
};

struct TestA{
	TestA(int _a = 1, short _b = 2, unsigned _c = 3):a(_a), b(_b), c(_c){}
	template <class S>
	void serialize(S &_s, Context &_rctx){
		cout<<"serialize TestA "<<_rctx.value<<endl;
		_s.push(a, "a::a").push(b, "a::b").push(c, "a::c");
	}
	int32 		a;
	int8 		b;
	uint32		c;
	void print()const{cout<<"testa: a = "<<a<<" b = "<<b<<" c = "<<c<<endl;}
};

struct TestB{
	TestB(int _a = 4):a(_a){}
	int32			a;
	void print()const {cout<<"testb: a = "<<a<<endl;}
	template <class S>
	void serialize(S &_s, Context &_rctx){
		cout<<"serialize TestB "<<_rctx.value<<endl;
		_s.push(a, "b::a");
	}
};

typedef serialization::binary::Serializer<Context>		BinSerializerT;
typedef serialization::binary::Deserializer<Context>	BinDeserializerT;
typedef serialization::IdTypeMapper<
	BinSerializerT, BinDeserializerT, uint16,
	serialization::FakeMutex
>														UInt16TypeMapperT;

struct Handle{
	bool checkStore(void *, Context &_rctx)const{
		return true;
	}
	
	void beforeSerialization(BinSerializerT &_rs, void *_pt, Context &_rctx){
		
	}
	
	void beforeSerialization(BinDeserializerT &_rs, void *_pt, Context &_rctx){
		
	}
	
	bool checkLoad(TestA *_pt, Context &_rctx)const{
		if(_rctx.value == 11){
			_rctx.value = 12;
			return true;
		}
		return false;
	}
	void afterSerialization(BinSerializerT &_rs, void *_pt, Context &_rctx){
	}
	void afterSerialization(BinDeserializerT &_rs, TestA *_pt, Context &_rctx){
		_pt->print();
		cout<<" _rctx.value = "<<_rctx.value<<endl;
	}
	bool checkLoad(TestB *_pt, Context &_rctx)const{
		if(_rctx.value == 10){
			_rctx.value = 11;
			return true;
		}
		return false;
	}
	void afterSerialization(BinDeserializerT &_rs, TestB *_pt, Context &_rctx){
		_pt->print();
		cout<<" _rctx.value = "<<_rctx.value<<endl;
	}
};

int main(int argc, char *argv[]){
#ifdef UDEBUG
	Debug::the().levelMask();
	Debug::the().moduleMask();
	Debug::the().initStdErr(false);
#endif
	const int				blen = 16;
	char					bufs[1000][blen];
	int						rv;
	UInt16TypeMapperT		tm;
	
	tm.insertHandle<TestA, Handle>();
	tm.insertHandle<TestB, Handle>();
	
	{
		BinSerializerT 	ser(tm);
		TestA 			*pta = new TestA;
		TestB 			*ptb = new TestB;
		int				v = 0, cnt = 0;
		Context			ctx;
		
		ser.push(pta, "testa").push(ptb, "testb");
		
		while((rv = ser.run(bufs[v], blen, ctx)) == blen){
			cnt += rv;
			++v;
		}
		if(rv < 0){
			cout<<"ERROR: serialization: "<<ser.errorString()<<endl;
			return 0;
		}else{
			cnt += rv;
		}
		cout<<"Write size = "<<cnt<<endl;
	}
	cout<<"Deserialization: =================================== "<<endl;
	{
		BinDeserializerT	des(tm);
		TestA				*pta = NULL;
		TestB				*ptb = NULL;
		int					v = 0;
		int					cnt = 0;
		Context				ctx;
		
		des.push(pta, "testa").push(ptb, "testb");
		
		while((rv = des.run(bufs[v], blen, ctx)) == blen){
			cnt += rv;
			++v;
		}
		
		if(rv < 0){
			cout<<"ERROR: deserialization "<<des.errorString()<<endl;
			return 0;
		}
		cnt += rv;
		cout<<"Read size = "<<cnt<<endl;
	}
	return 0;
}
