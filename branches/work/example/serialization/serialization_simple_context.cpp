// serialization_simple.cpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
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
#include "utility/dynamicpointer.hpp"
#include "utility/dynamictype.hpp"

using namespace std;
using namespace solid;

struct Context{
	Context(const char *_s, int _i):s(_s), i(_i){}
	void print(){
		cout<<"Context: "<<s<<' '<<i<<endl;
	}
	string	s;
	int 	i;
};

template <int T>
struct IndexType{
	enum{ Index = T};
};

///\cond 0
struct TestA{
	TestA(int _a = 1, short _b = 2, unsigned _c = 3):a(_a), b(_b), c(_c){}
	template <class S>
	void serialize(S &_s, Context &_rctx){
		_rctx.print();
		_s.push(a, "a::a").push(b, "a::b").push(c, "a::c");
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
	void serialize(S &_s, Context &_rctx){
		_rctx.print();
		_s.push(a, "b::a");
	}
};

struct TestC{
	explicit TestC(unsigned _a = 5):a(_a){}
	int32 	a;
	void print()const{cout<<"testc: a = "<<a<<endl;}
};

struct TestD{
	TestD(
		const char *_paddr = NULL,
		solid::uint _port = 0,
		int _a = 4
	):a(_a){
		if(_paddr){
			ResolveData rd = synchronous_resolve(_paddr, _port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
			if(!rd.empty()){
				sa = rd.begin();
			}
		}
	}
	int32				a;
	SocketAddressInet4	sa;
	void print()const {
		cout<<"testd: a  = "<<a<<endl;
		string				hoststr;
		string				portstr;
		synchronous_resolve(
			hoststr,
			portstr,
			sa,
			ReverseResolveInfo::Numeric
		);
		cout<<"testd: sa = "<<hoststr<<':'<<portstr<<endl;
		
	}
	template <class S>
	void serialize(S &_s, Context &_rctx){
		_rctx.print();
		_s.push(a, "b::a");
		const SocketAddressInet4 &rsa = sa;
		const sockaddr *psa = rsa;
		_s.pushBinary((void*)psa, SocketAddressInet4::Capacity, "sockaddr");
	}
};


struct Base: Dynamic<Base>{
	virtual ~Base(){}
	virtual void print() const = 0;
};

struct String: Base{
	String():dflt(false){}
	String(const IndexType<1>&):dflt(true), str("default"){}
	String(const char *_str):dflt(false), str(_str){}
	template <class S>
	void serialize(S &_s, Context &_rctx){
		_rctx.print();
		if(dflt){
			//_s;
		}else{
			_s.pushUtf8(str, "String::str");
		}
	}
	void print()const{
		cout<<"String: "<<str<<endl;
	}
	void makeDefault(){
		dflt = true;
	}
private:
	bool	dflt;
	string	str;
};

struct Integer: Base{
	Integer(int _i = 0):tc(_i){}
	template <class S>
	void serialize(S &_s, Context &_rctx){
		_rctx.print();
		_s.push(tc, "tc");
	}
	virtual void print()const{
		cout<<"Integer: "<<tc.a<<endl;
	}
private:
	TestC	tc;
};

struct UnsignedInteger: Integer{
	UnsignedInteger(int _i = 0, unsigned _u = 0):Integer(_i),u(_u){}
	template <class S>
	void serialize(S &_s, Context &_rctx){
		_rctx.print();
		_s.push(u, "String::str");
		Integer::serialize(_s, _rctx);
	}
	void print()const{
		cout<<"Unsigned Integer{"<<endl;
		cout<<"Unsigned: "<<u<<endl;
		Integer::print();
		cout<<"}"<<endl;
	}
private:
	uint32 	u;
};

struct IntegerVector: Base{
	typedef vector<uint32> 	IntVecT;
	IntegerVector():piv1(NULL), piv2(NULL) {
	}
	IntegerVector(bool):piv1(new IntVecT), piv2(NULL) {
		piv1->push_back(1);
		piv1->push_back(2);
	}
	void print()const;
	IntVecT	iv;
	IntVecT	*piv1;
	IntVecT	*piv2;
};

void IntegerVector::print()const{
	cout<<"IntegerVector{"<<endl;
	for(IntVecT::const_iterator it(iv.begin()); it != iv.end(); ++it){
		cout<<*it<<',';
	}
	cout<<endl;
	cout<<"piv1 = "<<(void*)piv1<<" piv2 = "<<(void*)piv2<<endl;
	cout<<"piv1->size = "<<piv1->size()<<endl;
	cout<<"piv1[0] = "<<(*piv1)[0]<<" piv1[0] = "<<(*piv1)[1]<<endl;
	cout<<'}'<<endl;
}
namespace solid{namespace serialization{namespace binary{
template <class S>
void serialize(S &_s, IntegerVector &_iv, Context &_rctx){
	_rctx.print();
	_s.pushContainer(_iv.iv, "IntegerVector::iv").pushContainer(_iv.piv1,"piv1").pushContainer(_iv.piv2, "piv2");
}
/*binary*/}/*serialization*/}/*solid*/}

struct Array: Base{
	Array(){
		pta = NULL;
		ptasz = -1;
		pta1 = (TestA*)1;
		pta1sz = -1;
	}
	Array(bool){
		sasz = 3;
		sa[0] = "first";
		sa[1] = "second";
		sa[2] = "third";
		pta1 = NULL;
		pta1sz = 0;
		pta = new TestA[10];
		ptasz = 10;
		for(int i(0); i < 10; ++i){
			pta[i].a += i;
			pta[i].b += i;
			pta[i].c += i;
		}
		td[0] = TestD("192.168.200.1",1111, 1);
		td[1] = TestD("192.168.200.2",1112, 2);
		td[2] = TestD("192.168.200.3",1113, 3);
		td[3] = TestD("192.168.200.4",1114, 4);
		tdsz = 4;
	}
	~Array(){
		delete []pta;
		delete []pta1;
	}
	template <class S>
	void serialize(S &_s, Context &_rctx){
		_rctx.print();
		_s.pushArray(sa, sasz, "sa");
		_s.pushDynamicArray(pta, ptasz, "pta");
		_s.pushDynamicArray(pta1, pta1sz, "pta1");
		_s.pushArray(td, tdsz, "td");
	}
	void print()const;
	
	std::string sa[3];
	size_t		sasz;
	
	TestA		*pta;
	size_t		ptasz;
	
	TestA		*pta1;
	size_t		pta1sz;
	
	TestD		td[4];
	size_t		tdsz;
};

void Array::print() const{
	cout<<"Array{"<<endl;
	cout<<"sasz = "<<(int)sasz<<endl;
	for(int i(0); i < sasz; ++i){
		cout<<"\""<<sa[i]<<"\" ";
	}
	cout<<endl;
	cout<<"ptasz = "<<ptasz<<endl;
	cout<<"pta = "<<(void*)pta<<"{"<<endl;
	for(int i(0); i < ptasz; ++i){
		pta[i].print();
	}
	cout<<"}pta"<<endl;
	cout<<"pta1 = "<<(void*)pta1<<"{"<<endl;
	for(int i(0); i < pta1sz; ++i){
		pta1[i].print();
	}
	cout<<"}pta1"<<endl;
	cout<<"tdsz = "<<tdsz<<endl;
	cout<<"td{"<<endl;
	for(size_t i(0); i < tdsz; ++i){
		td[i].print();
	}
	cout<<"}td"<<endl;
	cout<<"}array"<<endl;
}

namespace solid{namespace serialization{namespace binary{
template <class S>
void serialize(S &_s, Base &, Context &_rctx){
	cassert(false);
}

template <class S>
void serialize(S &_s, TestC &_tb, Context &_rctx){
	_rctx.print();
	_s.push(_tb.a, "c::a");
}

template <class S>
void serialize(S &_s, pair<int32,int32> &_tb, Context &_rctx){
	_rctx.print();
	_s.push(_tb.first, "first").push(_tb.second, "second");
}
/*binary*/}/*serialization*/}/*solid*/}

typedef std::deque<std::string> StrDeqT;
typedef std::deque<std::pair<int32,int32> > PairIntDeqT;

void print(StrDeqT &_rsdq);
///\endcond


enum{
	STRING_TYPE_INDEX = 1,
	STRING_DEFAULT_TYPE_INDEX,
	UNSIGNED_TYPE_INDEX,
	INTEGER_VECTOR_TYPE_INDEX,
	ARRAY_TYPE_INDEX
};

int main(int argc, char *argv[]){
	Thread::init();
#ifdef UDEBUG
	Debug::the().levelMask();
	Debug::the().moduleMask();
	Debug::the().initStdErr(false);
#endif
	cout<<"sizeof(map<int , string>::iterator): "<<sizeof(map<int32 , string>::iterator)<<endl;
	cout<<"sizeof(list<string>::iterator): "<<sizeof(list<string>::iterator)<<endl;
	cout<<"sizeof(deque<string>::iterator): "<<sizeof(deque<string>::iterator)<<endl;
	cout<<"sizeof(uint64) = "<<sizeof(uint64)<<endl;
	cout<<"sizeof(uint32) = "<<sizeof(uint32)<<endl;
	cout<<"sizeof(uint16) = "<<sizeof(uint16)<<endl;
	cout<<"sizeof(uint8) = "<<sizeof(uint8)<<endl;
	const int blen = 16;
	char bufs[1000][blen];
	int rv = 3;
	//TestA ta;
	//cout<<ta<<endl;
	typedef serialization::binary::Serializer<Context>								BinSerializerT;
	typedef serialization::binary::Deserializer<Context>							BinDeserializerT;
	typedef serialization::IdTypeMapper<BinSerializerT, BinDeserializerT, uint16>	UInt16TypeMapperT;
	
	
	UInt16TypeMapperT		tm;
	
	
	tm.insert<String>(STRING_TYPE_INDEX);
	tm.insert<String, IndexType<1> >(STRING_DEFAULT_TYPE_INDEX);
	tm.insert<UnsignedInteger>(UNSIGNED_TYPE_INDEX);
	tm.insert<IntegerVector>(INTEGER_VECTOR_TYPE_INDEX);
	tm.insert<Array>(ARRAY_TYPE_INDEX);
	//const char* str = NULL;
	for(int i = 0; i < 1; ++i){
		{	
			idbg("");
			Context			ctx("Serialization", 1000);
			BinSerializerT 	ser(tm);
			
			TestA 			ta;
			TestB 			tb;// = new TestB;
			TestC 			tc;
			StrDeqT			sdq;
			string			s("some string");
			
			sdq.push_back("first");
			sdq.push_back("second");
			sdq.push_back("third");
			sdq.push_back("fourth");
			
			Base			*b1 = new String("some mega base string that is more than few buffers spawned");
			Base			*b2 = new UnsignedInteger(-2, 10);
			IntegerVector	*iv;
			Base			*b3 = iv = new IntegerVector(true);
			Base			*b4 = new Array(true);
			
			for(int i = 1; i < 20; ++i){
				iv->iv.push_back(i);
			}
			//static_cast<String*>(b1)->makeDefault();
			
			ta.print();
			tb.print();
			tc.print();
			
			cout<<"string: "<<s<<endl;
			print(sdq);
			
			b1->print();
			b2->print();
			b3->print();
			b4->print();
			
			ser.push(ta, "testa").push(tb, "testb").push(tc, "testc");
			idbg("");
			ser.push(s, "string").pushContainer(sdq, "names");
			idbg("");
			ser.push(b1, /*tm, STRING_DEFAULT_TYPE_INDEX,*/ "basestring").push(b2, "baseui").push(b3, "baseiv").push(b4, "basea");
			
			PairIntDeqT pidq;
			pidq.push_back(pair<int32, int32>(1,2));
			pidq.push_back(pair<int32, int32>(2,3));
			pidq.push_back(pair<int32, int32>(3,4));
			ser.pushContainer(pidq, "pidq");
			pair<int32,int32> ppi(1,2);
			ser.push(ppi, "pi");
			for(PairIntDeqT::const_iterator it(pidq.begin()); it != pidq.end(); ++it){
				cout<<"("<<it->first<<','<<it->second<<')';
			}
			cout<<endl;
			int v = 0, cnt = 0;
			idbg("");
			while((rv = ser.run(bufs[v], blen, ctx)) == blen){
				cnt += rv;
				++v;
			}
			if(rv < 0){
				cout<<"ERROR: serialization: "<<ser.errorString()<<endl;
				return 0;
			}
			idbg("");
			cnt += rv;
			cout<<"Write count: "<<cnt<<" buffnct = "<<v<<endl;
		}
		cout<<"Deserialization: =================================== "<<endl;
		{
			Context					ctx("Deserialization", 1111);
			BinDeserializerT		des(tm);
			TestA					ta;
			TestB					tb;// = new TestB;
			TestC					tc;
			StrDeqT					sdq;
			string					s;
			DynamicPointer<Base>	b1;
			Base					*b2 = NULL;
			Base					*b3 = NULL;
			Base					*b4 = NULL;
			
			des.push(ta, "testa").push(tb, "testb").push(tc, "testc");
			idbg("");
			des.push(s, "string").pushContainer(sdq, "names");
			idbg("");
			des.pushStringLimit();
			des.push(b1, "basestring");
			des.pushStringLimit(100);
			des.push(b2, "baseui").push(b3, "baseiv").push(b4, "basea");
			idbg("");
			int v = 0;
			int cnt = 0;
			PairIntDeqT pidq;
			des.pushContainer(pidq, "pidq");
			pair<int32,int32> ppi;
			des.push(ppi, "pi");
			while((rv = des.run(bufs[v], blen, ctx)) == blen){
				cnt += rv;
				++v;
			}
			if(rv < 0){
				cout<<"ERROR: deserialization "<<des.errorString()<<endl;
				return 0;
			}
			cnt += rv;
			cout<<"Read count: "<<cnt<<endl;
			ta.print();
			tb.print();
			tc.print();
			cout<<"string: "<<s<<endl;
			print(sdq);
			if(b1.get()){
				cout<<"Dynamic pointer:"<<endl;
				b1->print();
			}
			if(b2)b2->print();
			b3->print();
			if(b4)b4->print();
			for(PairIntDeqT::const_iterator it(pidq.begin()); it != pidq.end(); ++it){
				cout<<"("<<it->first<<','<<it->second<<')';
			}
			cout<<endl;
			cout<<"pi.first "<<ppi.first<<" pi.second "<<ppi.second<<endl;
		}
	}//for
	idbg("Done");
	return 0;
}

void print(StrDeqT &_rsdq){
	cout<<"string deque begin:"<<endl;
	for(StrDeqT::const_iterator it(_rsdq.begin()); it != _rsdq.end(); ++it){
		cout<<*it<<endl;
	}
	cout<<"string deque end"<<endl;
}

