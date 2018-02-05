#include "solid/serialization/v2/typetraits.hpp"
#include "solid/serialization/serialization.hpp"
#include <vector>
#include <deque>
#include <string>
#include <sstream>
#include <iostream>

using namespace std;
using namespace solid;

template <typename T>
void add_impl(T &_rt, std::true_type){
    cout<<"add container"<<endl;
}

template <typename T>
void add_impl(T &_rt, std::false_type){
    cout<<"add non container"<<endl;
}


template <typename T>
void add(T &_rt){
    add_impl(_rt, serialization::is_container<T>());
}

struct A{
    int     b;
};

struct Context{
    
};

class Test{
    bool        b;
    vector<A>   v;
    deque<A>    d;
    A           a;
    
    void populate(){
        
    }
    
public:
    Test(){}
    
    Test(bool){
        populate();
    }
    
    template <class S>
    void solidSerializeV2(S &_rs){
        _rs.add(b, "b").add(
            [this](S &_rs){
                if(this->b){
                    _rs.add(v, "v");
                }else{
                    _rs.add(d, "d");
                }
            }
        ).add(a, "a");
    }
    
    template <class S>
    void solidSerializeV2(S &_rs, Context &_rctx){
        _rs.add(b, "b").add(
            [this](S &_rs, Context &_rctx){
                if(this->b){
                    _rs.add(v, "v");
                }else{
                    _rs.add(d, "d");
                }
            }
        ).add(a, "a");
    }
};

int test_binary(int argc, char* argv[]){
    {
        vector<A>   v;
        A           a;
        deque<A>    d;
        add(a);
        add(v);
        add(d);
    }
    {
        const Test t{true};
        
        ostringstream oss;
        {
            serialization::binary::Serializer<> ser;
            
            ser.add(t, "t");
            
            oss<<ser;
        }
        {
            istringstream iss(oss.str());
            serialization::binary::Deserializer<Context> des;
            
            Test    t_c;
            Context ctx;
            
            des.add(t_c, ctx, "t");
            
            iss>>des.wrap(ctx);
        }
    }
    return 0;
}

