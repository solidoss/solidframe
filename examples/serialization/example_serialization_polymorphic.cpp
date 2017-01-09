#include "solid/serialization/binary.hpp"


#include <iostream>
#include <string>

using namespace solid;
using namespace std;

struct Base{
    Base(uint64_t _val = 0): val(_val){}
    uint64_t        val;
    virtual void print()const = 0;
};

struct TestA: Base{
    TestA(int _a = 1, short _b = 2, unsigned _c = 3):a(_a), b(_b), c(_c){}
    template <class S>
    void serialize(S &_s){
        _s.push(a, "a::a").push(b, "a::b").push(c, "a::c");
    }
    int32_t         a;
    int16_t         b;
    uint32_t        c;
    void print()const{cout<<"testa: a = "<<a<<" b = "<<b<<" c = "<<c<<" Base::value = "<<val<<endl;}
};

struct TestB: Base{
    TestB(int _a = 4, size_t _val = 8):Base(_val), a(_a){}
    int32_t         a;
    void print()const {cout<<"testb: a = "<<a<<" Base::value = "<<val<<endl;}
    template <class S>
    void serialize(S &_s){
        _s.push(a, "b::a");
    }
};

typedef serialization::binary::Serializer<void>                                 BinSerializerT;
typedef serialization::binary::Deserializer<void>                               BinDeserializerT;
typedef serialization::TypeIdMap<BinSerializerT, BinDeserializerT, std::string> TypeIdMapT;


template <class S, class T>
void serialize(S &_rs, T &_rt, const char *_name){
    _rs.pushCross(static_cast<Base&>(_rt).val, "base::value");
    _rs.push(_rt, _name);
}

typedef std::shared_ptr<Base>                                                   BasePointerT;

int main(){

    string      data;

    TypeIdMapT  typemap;

    typemap.registerType<TestA>("testa");
    typemap.registerType<TestB>("testb", serialize<BinSerializerT, TestB>, serialize<BinDeserializerT, TestB>);
    typemap.registerCast<TestA, Base>();
    typemap.registerCast<TestB, Base>();

    {
        const size_t        bufcp = 64;
        char                buf[bufcp];
        BinSerializerT      ser(&typemap);
        int                 rv;

        TestA               a;

        Base                *pa = &a;
        BasePointerT        bptr(new TestB(2, 4));

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
        BinDeserializerT    des(&typemap);
        int                 rv;

        Base                *pa = nullptr;
        BasePointerT        bptr;

        des.push(pa, "pa").push(bptr, "pb");

        rv = des.run(data.data(), data.size());

        if(rv != static_cast<int>(data.size())){
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
