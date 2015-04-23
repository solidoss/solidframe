#include "serialization/binary.hpp"

#include <iostream>
#include <string>

using namespace solid;
using namespace std;

struct Base{
	virtual ~Base(){}
};

struct TestA: Base{
	TestA(int _a = 1, short _b = 2, unsigned _c = 3):a(_a), b(_b), c(_c){}
	template <class S>
	void serialize(S &_s){
		_s.push(a, "a::a").push(b, "a::b").push(c, "a::c");
	}
	int32 		a;
	int16 		b;
	uint32		c;
	void print()const{cout<<"testa: a = "<<a<<" b = "<<b<<" c = "<<c<<endl;}
};

struct TestB: Base{
	TestB(int _a = 4):a(_a){}
	int32			a;
	void print()const {cout<<"testb: a = "<<a<<endl;}
	template <class S>
	void serialize(S &_s){
		_s.push(a, "b::a");
	}
};

typedef serialization::binary::Serializer<void>									BinSerializerT;
typedef serialization::binary::Deserializer<void>								BinDeserializerT;
typedef serialization::TypeIdMap<BinSerializerT, BinDeserializerT>				TypeIdMapT;

int main(){
	
	string		data;
	
	TypeIdMapT	typemap;
	
	typemap.registerType<TestA>();
	typemap.registerType<TestB>();
	
	{
		const size_t		bufcp = 64;
		char 				buf[bufcp];
		BinSerializerT		ser;
		int					rv;
		
		TestA				a;
		TestB				b;
		
		ser.push(a, "a").push(b, "b");
		
		while((rv = ser.run(buf, bufcp)) == bufcp){
			data.append(buf, rv);
		}
		if(rv < 0){
			cout<<"ERROR: serialization: "<<ser.errorString()<<endl;
			return 0;
		}else{
			data.append(buf, rv);
		}
	}
	{
		BinDeserializerT	des;
		TestA				a(0,0,0);
		TestB				b(0);
		int					rv;
		
		des.push(a, "a").push(b, "b");
		
		rv = des.run(data.data(), data.size());
		if(rv != data.size()){
			cout<<"ERROR: deserialization: "<<des.errorString()<<endl;
			return 0;
		}
		a.print();
		b.print();
	}
	return 0;
}