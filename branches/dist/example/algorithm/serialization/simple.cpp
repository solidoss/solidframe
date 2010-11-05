/* Implementation file simple_new.cpp
	
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

#include <iostream>
#include <stack>
#include <vector>
#include <deque>
#include <map>
#include <list>
//#undef UDEBUG
#include "system/thread.hpp"
#include "system/debug.hpp"
#include "algorithm/serialization/typemapper.hpp"
#include "algorithm/serialization/binary.hpp"
#include "algorithm/serialization/idtypemap.hpp"
using namespace std;

// template <class S>
// S& operator&(string &, S &_s){
// 	cassert(false);
// 	return _s;
// }
// 
// template <class S>
// S& operator&(unsigned &, S &_s){
// 	cassert(false);
// 	return _s;
// }
// 
// template <class S>
// S& operator&(ulong &, S &_s){
// 	cassert(false);
// 	return _s;
// }
// 
// template <class S>
// S& operator&(int &, S &_s){
// 	cassert(false);
// 	return _s;
// }
// 
// template <class S>
// S& operator&(short &, S &_s){
// 	cassert(false);
// 	return _s;
// }
///\cond 0
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

struct TestC{
	explicit TestC(unsigned _a = 5):a(_a){}
	int32 	a;
	void print()const{cout<<"testc: a = "<<a<<endl;}
};

struct Base{
	virtual ~Base(){}
	virtual void print() const = 0;
};

struct String: Base{
	String(){}
	String(const char *_str):str(_str){}
	template <class S>
	S& operator&(S &_s){
		return _s.push(str, "String::str");
	}
	void print()const{
		cout<<"String: "<<str<<endl;
	}
private:
	string	str;
};

struct Integer: Base{
	Integer(int _i = 0):tc(_i){}
	template <class S>
	S& operator&(S &_s){
		return _s.push(tc, "tc");
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
	S& operator&(S &_s){
		_s.push(u, "String::str");
		return Integer::operator&(_s);
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

template <class S>
S& operator&(IntegerVector &_iv, S &_s){
	return _s.pushContainer(_iv.iv, "IntegerVector::iv").pushContainer(_iv.piv1,"piv1").pushContainer(_iv.piv2, "piv2");
	//return _s;
}

template <class S>
S& operator&(Base &, S &_s){
	cassert(false);
	return _s;
}

template <class S>
S& operator&(TestC &_tb, S &_s){
	return _s.push(_tb.a, "c::a");
}
namespace std{
template <class S>
S& operator&(pair<int32,int32> &_tb, S &_s){
	return _s.push(_tb.first, "first").push(_tb.second, "second");
}
}

typedef std::deque<std::string> StrDeqT;
typedef std::deque<std::pair<int32,int32> > PairIntDeqT;

void print(StrDeqT &_rsdq);
///\endcond

int main(int argc, char *argv[]){
	Thread::init();
#ifdef UDEBUG
	Dbg::instance().levelMask();
	Dbg::instance().moduleMask();
	Dbg::instance().initStdErr();
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
	typedef serialization::TypeMapper					TypeMapper;
	//typedef serialization::NameTypeMap					NameTypeMap;
	typedef serialization::IdTypeMap					IdTypeMap;
	typedef serialization::bin::Serializer				BinSerializer;
	typedef serialization::bin::Deserializer			BinDeserializer;
	
	TypeMapper::registerMap<IdTypeMap>(new IdTypeMap);
	
	TypeMapper::registerSerializer<BinSerializer>();
	
	TypeMapper::map<String, BinSerializer, BinDeserializer>();
	TypeMapper::map<UnsignedInteger, BinSerializer, BinDeserializer>();
	TypeMapper::map<IntegerVector, BinSerializer, BinDeserializer>();
	//const char* str = NULL;
	{	
		idbg("");
		BinSerializer 	ser(IdTypeMap::the());
		
		TestA 			ta;
		TestB 			tb;// = new TestB;
		TestC 			tc;
		StrDeqT		sdq;
		string			s("some string");
		
		sdq.push_back("first");
		sdq.push_back("second");
		sdq.push_back("third");
		sdq.push_back("fourth");
		
		Base			*b1 = new String("some base string");
		Base			*b2 = new UnsignedInteger(-2, 10);
		IntegerVector	*iv;
		Base			*b3 = iv = new IntegerVector(true);
		
		for(int i = 1; i < 20; ++i){
			iv->iv.push_back(i);
		}
		
		ta.print();
		tb.print();
		tc.print();
		
		cout<<"string: "<<s<<endl;
		print(sdq);
		
		b1->print();
		b2->print();
		b3->print();
		
		ser.push(ta, "testa").push(tb, "testb").push(tc, "testc");
		idbg("");
		ser.push(s, "string").pushContainer(sdq, "names");
		idbg("");
		ser.push(b1, "basestring").push(b2, "baseui").push(b3, "baseiv");
		
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
		while((rv = ser.run(bufs[v], blen)) == blen){
			cnt += rv;
			++v;
		}
		idbg("");
		cnt += rv;
		cout<<"Write count: "<<cnt<<endl;
	}
	cout<<"Deserialization: =================================== "<<endl;
	{
		BinDeserializer des(IdTypeMap::the());
		TestA		ta;
		TestB		tb;// = new TestB;
		TestC		tc;
		StrDeqT	sdq;
		string		s;
		Base		*b1 = NULL;
		Base		*b2 = NULL;
		Base		*b3 = NULL;
		
		des.push(ta, "testa").push(tb, "testb").push(tc, "testc");
		idbg("");
		des.push(s, "string").pushContainer(sdq, "names");
		idbg("");
		des.push(b1, "basestring").push(b2, "baseui").push(b3, "baseiv");
		idbg("");
		int v = 0;
		int cnt = 0;
		PairIntDeqT pidq;
		des.pushContainer(pidq, "pidq");
		pair<int32,int32> ppi;
		des.push(ppi, "pi");
		while((rv = des.run(bufs[v], blen)) == blen){
			cnt += rv;
			++v;
		}
		cnt += rv;
		cout<<"Read count: "<<cnt<<endl;
		ta.print();
		tb.print();
		tc.print();
		cout<<"string: "<<s<<endl;
		print(sdq);
		if(b1)b1->print();
		if(b2)b2->print();
		b3->print();
		for(PairIntDeqT::const_iterator it(pidq.begin()); it != pidq.end(); ++it){
			cout<<"("<<it->first<<','<<it->second<<')';
		}
		cout<<endl;
		cout<<"pi.first "<<ppi.first<<" pi.second "<<ppi.second<<endl;
	}
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

