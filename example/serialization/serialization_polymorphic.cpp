#include "serialization/binary.hpp"
#include "utility/dynamictype.hpp"

#include <iostream>
#include <string>

using namespace solid;
using namespace std;

struct Base: Dynamic<Base>{
	virtual void print()const = 0;
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
typedef serialization::TypeIdMap<BinSerializerT, BinDeserializerT, std::string>	TypeIdMapT;

typedef DynamicPointer<Base>													BasePointerT;

int main(){
	
	string		data;
	
	TypeIdMapT	typemap;
	
	typemap.registerType<TestA>("testa", serialization::basic_factory<TestA>);
	typemap.registerType<TestB>("testb", serialization::basic_factory<TestB>);
	typemap.registerCast<TestA, Base>();
	typemap.registerCast<TestB, Base>();
	
	{
		const size_t		bufcp = 64;
		char 				buf[bufcp];
		BinSerializerT		ser(&typemap);
		int					rv;
		
		TestA				a;
		
		Base				*pa = &a;
		BasePointerT		bptr = new TestB;
		
		cout<<"Typename pa: "<<typeid(*pa).name()<<endl;
		
		ser.push(pa, "pa").push(bptr, "pb");
		
		while((rv = ser.run(buf, bufcp)) == bufcp){
			data.append(buf, rv);
		}
		if(rv < 0){
			cout<<"ERROR: serialization: "<<ser.error().category().name()<<": "<<ser.error().message()<<endl;
			return 0;
		}else{
			data.append(buf, rv);
		}
	}
	{
		BinDeserializerT	des(&typemap);
		int					rv;
		
		Base				*pa = nullptr;
		BasePointerT		bptr;
		
		des.push(pa, "pa").push(bptr, "pb");
		
		rv = des.run(data.data(), data.size());
		
		if(rv != data.size()){
			cout<<"ERROR: deserialization: "<<des.error().category().name()<<": "<<des.error().message()<<endl;
			return 0;
		}
		
		cout<<"Data for pa = "<<typemap[pa]<<endl;
		cout<<"Data for pb = "<<typemap[bptr.get()]<<endl;
		
		pa->print();
		bptr->print();
	}
	return 0;
}