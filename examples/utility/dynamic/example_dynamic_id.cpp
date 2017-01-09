#include <iostream>
#include "solid/system/debug.hpp"
#include "solid/utility/dynamictype.hpp"

using namespace std;
using namespace solid;

struct A: Dynamic<A>{
    A(int _v):v(_v){}
    int v;
};

struct B: Dynamic<B, A>{
    B(int _v): BaseT(_v){}

};

struct C: Dynamic<C, B>{
    C(int _v): BaseT(_v){}

};

struct D: Dynamic<D, C>{
    D(int _v): BaseT(_v){}

};


int main(int argc, char *argv[]){
    cout<<"typeid A: "<<A::staticTypeId()<<endl;
    cout<<"typeid B: "<<B::staticTypeId()<<endl;
    cout<<"typeid C: "<<C::staticTypeId()<<endl;
    cout<<"typeid D: "<<D::staticTypeId()<<endl;

    DynamicIdVectorT        idvec;

    D::staticTypeIds(idvec);
    cout<<"typeids: ";
    for(auto it = idvec.begin(); it != idvec.end(); ++it){
        cout<<*it<<" ";
    }
    cout<<endl;

    D   d(10);
    A   &a(d);

    idvec.clear();
    a.dynamicTypeIds(idvec);

    cout<<"typeids: ";
    for(auto it = idvec.begin(); it != idvec.end(); ++it){
        cout<<*it<<" ";
    }
    cout<<endl;
    return 0;
}
