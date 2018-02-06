#include "solid/serialization/v2/typetraits.hpp"
#include "solid/serialization/serialization.hpp"
#include <vector>
#include <deque>
#include <string>
#include <sstream>
#include <iostream>
#include <type_traits>

using namespace std;
using namespace solid;

struct A{
    int32_t     a;
    uint64_t    b;
};

template <class S>
void solidSerializeV2(S &_rs, const A &_r, const char *_name){
    _rs.add(_r.a, "a").add(_r.b, "b");
}

template <class S, class Ctx>
void solidSerializeV2(S &_rs, A &_r, Ctx &_rctx, const char *_name){
    _rs.add(_r.a, _rctx, "a").add(_r.b, _rctx, "b");
}

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
    void solidSerializeV2(S &_rs, const char *_name)const{
        _rs.add(b, "b").add(
            [this](S &_rs, const char* _name){
                if(this->b){
                    _rs.add(v, "v");
                }else{
                    _rs.add(d, "d");
                }
            }, "f"
        ).add(a, "a");
        //_rs.add(b, "b").add(v, "v").add(a, "a");
    }
    
    template <class S>
    void solidSerializeV2(S &_rs, Context &_rctx, const char *_name){
        _rs.add(b, _rctx, "b").add(
            [this](S &_rs, Context &_rctx, const char*_name){
                if(this->b){
                    _rs.add(v, _rctx, "v");
                }else{
                    _rs.add(d, _rctx, "d");
                }
            }, _rctx, "f"
        ).add(a, _rctx, "a");
        //_rs.add(b, _rctx, "b").add(v, _rctx, "v").add(a, _rctx, "a");
    }
};

int test_binary(int argc, char* argv[]){
#ifdef SOLID_HAS_DEBUG
    Debug::the().levelMask("view");
    Debug::the().moduleMask("any");
    Debug::the().initStdErr(false, nullptr);
#endif
    
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

