/* Implementation file simple.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <stack>
#include <vector>
#include <deque>
#include <map>
#include <list>
//#undef UDEBUG
#include "system/thread.h"
#include "algorithm/serialization/binary.h"

using namespace std;


struct TestA{
	TestA(int _a = 1, short _b = 2, unsigned _c = 3):a(_a), b(_b), c(_c){}
	template <class S>
	S& operator&(S &_s){
		return _s.push(a, "a::a").push(b, "a::b").push(c, "a::c");
	}
	int 		a;
	short 		b;
	unsigned	c;
	void print()const{cout<<"testa: a = "<<a<<" b = "<<b<<" c = "<<c<<endl;}
};

struct TestB{
	TestB(int _a = 4):a(_a){}
	int			a;
	void print()const {cout<<"testb: a = "<<a<<endl;}
	template <class S>
	S& operator&(S &_s){
		return _s.push(a, "b::a");
	}
};

struct TestC{
	explicit TestC(unsigned _a = 5):a(_a){}
	int 	a;
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
	unsigned 	u;
};

struct IntegerVector: Base{
	IntegerVector(){}
	void print()const;
	typedef vector<int> 	IntVecTp;
	IntVecTp	iv;
};


void IntegerVector::print()const{
	cout<<"IntegerVector{"<<endl;
	for(IntVecTp::const_iterator it(iv.begin()); it != iv.end(); ++it){
		cout<<*it<<',';
	}
	cout<<'}'<<endl;
}

template <class S>
S& operator&(IntegerVector &_iv, S &_s){
	return _s.pushContainer(_iv.iv, "IntegerVector::iv");
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

typedef std::deque<std::string> StrDeqTp;

void print(StrDeqTp &_rsdq);

int main(int argc, char *argv[]){
	Thread::init();
#ifdef UDEBUG
	{
	string s = "dbg/";
	s+= argv[0]+2;
	initDebug(s.c_str());
	}
#endif
	cout<<"sizeof(map<int , string>::iterator): "<<sizeof(map<int , string>::iterator)<<endl;
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
	typedef serialization::bin::RTTIMapper 				BinMapper;
	typedef serialization::bin::Serializer<BinMapper>	BinSerializer;
	typedef serialization::bin::Deserializer<BinMapper>	BinDeserializer;
	BinMapper	fm;
	fm.map<String>();
	fm.map<UnsignedInteger>();
	fm.map<IntegerVector>();
	//const char* str = NULL;
	{	
		idbg("");
		BinSerializer 	ser(fm);
		TestA 		ta;
		TestB 		tb;// = new TestB;
		TestC 		tc;
		StrDeqTp	sdq;
		string		s("some string");
		sdq.push_back("first");
		sdq.push_back("second");
		sdq.push_back("third");
		sdq.push_back("fourth");
		Base	*b1 = new String("some base string");
		Base	*b2 = new UnsignedInteger(-2, 10);
		IntegerVector *iv;
		Base	*b3 = iv = new IntegerVector;
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
		//os<<t<<ptb<<ase::ni(tc);//ni = non intrusive
		ser.push(ta, "testa").push(tb, "testb").push(tc, "testc").pushString(s, "string").pushContainer(sdq, "names");
		ser.push(b1, "basestring").push(b2, "baseui").push(b3, "baseiv");
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
		BinDeserializer des(fm);
		TestA 		ta;
		TestB 		tb;// = new TestB;
		TestC 		tc;
		StrDeqTp	sdq;
		string 		s;
		Base	*b1 = NULL;
		Base	*b2 = NULL;
		Base	*b3 = NULL;
		//is>>ta>>ptb>>ase::ni(tc);
		//des.pushContainer(sdq, "names");
		des.push(ta, "testa").push(tb, "testb").push(tc, "testc").pushString(s, "string").pushContainer(sdq, "names");
		des.push(b1, "basestring").push(b2, "baseui").push(b3, "baseiv");
		int v = 0;
		int cnt = 0;
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
		b1->print();
		b2->print();
		b3->print();
	}
	
	return 0;
}

void print(StrDeqTp &_rsdq){
	cout<<"string deque begin:"<<endl;
	for(StrDeqTp::const_iterator it(_rsdq.begin()); it != _rsdq.end(); ++it){
		cout<<*it<<endl;
	}
	cout<<"string deque end"<<endl;
}

