/* Implementation file serialization_handle.cpp
	
	Copyright 2013 Valentin Palade 
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


struct TestA{
	TestA(int _a = 1, short _b = 2, unsigned _c = 3):a(_a), b(_b), c(_c){}
	template <class S>
	S& operator&(S &_s){
		return _s.push(a, "a::a").push(b, "a::b").push(c, "a::c");
	}
	int32 		a;
	int16 		b;
	uint32		c;
	void print()const{cout<<"testa: a = "<<a<<" b = "<<b<<" c = "<<c<<endl;}
};

struct TestB{
	TestB(int _a = 4):a(_a){}
	int32			a;
	void print()const {cout<<"testb: a = "<<a<<endl;}
	template <class S>
	S& operator&(S &_s){
		return _s.push(a, "b::a");
	}
};

struct Context{
	Context(int _v = 10):value(_v){}
	int value;
};

typedef serialization::binary::Serializer<Context>		BinSerializerT;
typedef serialization::binary::Deserializer<Context>	BinDeserializerT;
typedef serialization::IdTypeMapper<
	BinSerializerT, BinDeserializerT, uint16,
	serialization::FakeMutex
>														UInt16TypeMapperT;

struct Handle{
	bool checkStore(void *, Context *_pctx)const{
		return true;
	}
	
	bool checkLoad(TestA *_pt, Context* _pctx)const{
		if(_pctx->value == 11){
			_pctx->value = 12;
			return true;
		}
		return false;
	}
	void handle(TestA *_pt, Context *_pctx, const char *_name){
		_pt->print();
		cout<<_name<<" _pctx->value = "<<_pctx->value<<endl;
	}
	bool checkLoad(TestB *_pt, Context* _pctx)const{
		if(_pctx->value == 10){
			_pctx->value = 11;
			return true;
		}
		return false;
	}
	void handle(TestB *_pt, Context *_pctx, const char *_name){
		_pt->print();
		cout<<_name<<" _pctx->value = "<<_pctx->value<<endl;
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
		
		ser.push(pta, "testa").push(ptb, "testb");
		
		while((rv = ser.run(bufs[v], blen)) == blen){
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
		
		des.push(pta, &ctx, "testa").push(ptb, &ctx, "testb");
		
		while((rv = des.run(bufs[v], blen)) == blen){
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
