/* Implementation file serialization_reinit.cpp
	
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
#include <vector>

using namespace std;
using namespace solid;

struct Test{
	virtual ~Test(){}
	virtual void print()const = 0;
};

struct TestA: Test{
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

struct TestB: Test{
	TestB(int _a = 4):a(_a){}
	int32			a;
	void print()const {cout<<"testb: a = "<<a<<endl;}
	template <class S>
	S& operator&(S &_s){
		return _s.push(a, "b::a");
	}
};

class Container{
	typedef std::vector<Test*>	TestVectorT;
public:
	~Container(){
		clear();
	}
	void clear(){
		for(TestVectorT::const_iterator it(tstvec.begin()); it != tstvec.end(); ++it){
			delete *it;
		}
		tstvec.clear();
	}
	void print()const{
		for(TestVectorT::const_iterator it(tstvec.begin()); it != tstvec.end(); ++it){
			(*it)->print();
		}
	}
	void push(Test *_ptst){
		tstvec.push_back(_ptst);
	}
	template <class S>
	S& operator&(S &_s){
		
		return _s;
	}
private:
	TestVectorT tstvec;
};

typedef serialization::binary::Serializer<>							BinSerializerT;
typedef serialization::binary::Deserializer<>						BinDeserializerT;

int main(int argc, char *argv[]){
#ifdef UDEBUG
	Debug::the().levelMask();
	Debug::the().moduleMask();
	Debug::the().initStdErr(false);
#endif
	const int				blen = 16;
	char					bufs[1000][blen];
	int						rv;
	
	{
		BinSerializerT 	ser;
		int				v = 0, cnt = 0;
		Container		c;
		
		c.push(new TestA);
		c.push(new TestB);
		
		ser.push(c);
		
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
		BinDeserializerT	des;
		int					v = 0;
		int					cnt = 0;
		Container			c;
		
		des.push(c);
		
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
		
		c.print();
	}
	return 0;
}
