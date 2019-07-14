#include "solid/serialization/v2/serialization.hpp"
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <vector>


using namespace solid;
using namespace std;

#define PROTOCOL_V2(ser, rthis, ctx, name)                                                         \
    template <class S>                                                                                   \
    void solidSerializeV2(S& _s, Context& _rctx, const char* _name) const \
    {                                                                                                    \
        solidSerializeV2(_s, *this, _rctx, _name);                                                       \
    }                                                                                                    \
    template <class S>                                                                                   \
    void solidSerializeV2(S& _s, Context& _rctx, const char* _name)       \
    {                                                                                                    \
        solidSerializeV2(_s, *this, _rctx, _name);                                                       \
    }                                                                                                    \
    template <class S, class T>                                                                          \
    static void solidSerializeV2(S& ser, T& rthis, Context& ctx, const char* name)

struct Context {
};

struct TypeData {
};

namespace{

namespace v1{
struct Test{
    static constexpr uint32_t version = 1;
    
    struct NestedA{
        static constexpr uint32_t version = 1;
        uint32_t v_;
        
        PROTOCOL_V2(_rser, _rthis, _rctx, _name){
            _rser.add(_rthis.v_, _rctx, _name);
        }
    } nested_;
    
    PROTOCOL_V2(_rser, _rthis, _rctx, _name){
        _rser.template addVersion<Test>(Test::version);
        
        _rser.add(
            [&_rthis](S& _rser, Context& _rctx, const char* _name) {
                _rser. template addVersion<NestedA>(NestedA::version);
                _rser.add(_rthis.nested_, _rctx, _name);
            },
            _rctx, _name);
    }
};
}//namespace v1

namespace v2{
struct Test{
    static constexpr uint32_t version = 1;
    
    struct NestedA{
        static constexpr uint32_t version = 2;
        uint32_t v_i_;
        string   v_s_;
        
        PROTOCOL_V2(_rser, _rthis, _rctx, _name){
            if(_rser.version(_rthis) == 1){
                _rser.add(_rthis.v_i_, _rctx, _name);
            }else if(_rser.version(_rthis) == 1){
                _rser.add(_rthis.v_s_, _rctx, _name);
            }
        }
    } nested_;
    
    PROTOCOL_V2(_rser, _rthis, _rctx, _name){
        _rser.template addVersion<Test>(Test::version);
        
        _rser.add(
            [&_rthis](S& _rser, Context& _rctx, const char* _name) {
                _rser.template addVersion<NestedA>(NestedA::version);
                _rser.add(_rthis.nested_, _rctx, _name);
            },
            _rctx, _name);
    }
};
}//namespace v2

namespace v3{
struct Test{
    static constexpr uint32_t version = 2;
    
    struct NestedA{
        static constexpr uint32_t version = 2;
        uint32_t v_i_;
        string   v_s_;
        
        PROTOCOL_V2(_rser, _rthis, _rctx, _name){
            if(_rser.version(_rthis) == 1){
                _rser.add(_rthis.v_i_, _rctx, _name);
            }else if(_rser.version(_rthis) == 1){
                _rser.add(_rthis.v_s_, _rctx, _name);
            }
        }
    } nesteda_;
    
    struct NestedB{
        static constexpr uint32_t version = 1;
        string v_;
        
        PROTOCOL_V2(_rser, _rthis, _rctx, _name){
            _rser.add(_rthis.v_, _rctx, _name);
        }
        
    } nestedb_;
    
    PROTOCOL_V2(_rser, _rthis, _rctx, _name){
        _rser.template addVersion<Test>(Test::version);
        
        _rser.add(
            [&_rthis](S& _rser, Context& _rctx, const char* _name) {
                if(_rser.version(_rthis) == 1){
                    _rser.template addVersion<NestedA>(NestedA::version);
                    _rser.add(_rthis.nestedb_, _rctx, _name);
                }else if(_rser.version(_rthis) == 2){
                    _rser.template addVersion<NestedB>(NestedB::version);
                    _rser.add(_rthis.nestedb_, _rctx, _name);
                }
            },
            _rctx, _name);
    }
};
}//namespace v3


namespace v4{
struct Test{
    static constexpr uint32_t version = 2;
    
    struct NestedA{
        static constexpr uint32_t version = 2;
        
        uint32_t v_i_;
        string   v_s_;
        
        PROTOCOL_V2(_rser, _rthis, _rctx, _name){
            if(_rser.version(_rthis) == 1){
                _rser.add(_rthis.v_i_, _rctx, _name);
            }else if(_rser.version(_rthis) == 1){
                _rser.add(_rthis.v_s_, _rctx, _name);
            }
        }
    } nesteda_;
    
    struct NestedB{
        static constexpr uint32_t version = 2;
        uint64_t v_i_;
        string   v_s_;
        
        PROTOCOL_V2(_rser, _rthis, _rctx, _name){
            if(_rser.version(_rthis) == 1){
                _rser.add(_rthis.v_s_, _rctx, _name);
            }else if(_rser.version(_rthis) == 2){
                 _rser.add(_rthis.v_i_, _rctx, _name);
            }
        }
    } nestedb_;
    
    PROTOCOL_V2(_rser, _rthis, _rctx, _name){
        _rser.template addVersion<Test>(Test::version);
        
        _rser.add(
            [&_rthis](S& _rser, Context& _rctx, const char* _name) {
                if(_rser.version(_rthis) == 1){
                    _rser.template addVersion<NestedA>(NestedA::version);
                    _rser.add(_rthis.nestedb_, _rctx, _name);
                }else if(_rser.version(_rthis) == 2){
                    _rser.template addVersion<NestedB>(NestedB::version);
                    _rser.add(_rthis.nestedb_, _rctx, _name);
                }
            },
            _rctx, _name);
    }
    
    bool check(const v1::Test &_rt){
        return nesteda_.v_i_ == _rt.nested_.v_;
    }
    
    bool check(const v2::Test &_rt){
        return nesteda_.v_s_ == _rt.nested_.v_s_;
    }
    
    bool check(const v3::Test &_rt){
        return nestedb_.v_s_ == _rt.nestedb_.v_;
    }
    
    bool check(const v4::Test &_rt){
        return nestedb_.v_s_ == _rt.nestedb_.v_s_;
    }
};
}//namespace v4
    
}//namespace

int test_versioning(int /*argc*/, char* /*argv*/ [])
{

    namespace last = v4;
    
    solid::log_start(std::cerr, {".*:EW"});

    using TypeMapT      = serialization::TypeMap<uint8_t, Context, serialization::binary::Serializer, serialization::binary::Deserializer, TypeData>;
    using SerializerT   = TypeMapT::SerializerT;
    using DeserializerT = TypeMapT::DeserializerT;
    
    TypeMapT typemap;

    typemap.null(0);
    
    {
        stringstream ss;
        v1::Test t_v;
        {
            Context     ctx;
            SerializerT ser   = typemap.createSerializer();
            
            t_v.nested_.v_ = 10;
            ser.run(
                ss,
                [&t_v](decltype(ser)& ser, Context& _rctx){
                    ser.add(t_v, _rctx, "test");
                },
                ctx);
        }
        {
            using namespace last;
            Context         ctx;
            DeserializerT   des   = typemap.createDeserializer();
            
            Test t;
            
            des.run(
                ss,
                [&t](decltype(des)& des, Context& _rctx){
                    des.add(t, _rctx, "test");
                },
                ctx);
            solid_check(t.check(t_v));
        }
    }
    
    {
        stringstream ss;
        v2::Test t_v;
        {
            Context     ctx;
            SerializerT ser   = typemap.createSerializer();
            
            t_v.nested_.v_s_ = "test v2";
            
            ser.run(
                ss,
                [&t_v](decltype(ser)& ser, Context& _rctx){
                    ser.add(t_v, _rctx, "test");
                },
                ctx);
        }
        {
            using namespace last;
            Context         ctx;
            DeserializerT   des   = typemap.createDeserializer();
            
            Test t;
            
            des.run(
                ss,
                [&t](decltype(des)& des, Context& _rctx){
                    des.add(t, _rctx, "test");
                },
                ctx);
            solid_check(t.check(t_v));
        }
    }
    
    {
        stringstream ss;
        v3::Test t_v;
        {
            Context     ctx;
            SerializerT ser   = typemap.createSerializer();
            
            t_v.nestedb_.v_ = "test v3";
            
            ser.run(
                ss,
                [&t_v](decltype(ser)& ser, Context& _rctx){
                    ser.add(t_v, _rctx, "test");
                },
                ctx);
        }
        {
            using namespace last;
            Context         ctx;
            DeserializerT   des   = typemap.createDeserializer();
            
            Test t;
            
            des.run(
                ss,
                [&t](decltype(des)& des, Context& _rctx){
                    des.add(t, _rctx, "test");
                },
                ctx);
            solid_check(t.check(t_v));
        }
    }
    
    {
        stringstream ss;
        v4::Test t_v;
        {
            Context     ctx;
            SerializerT ser   = typemap.createSerializer();
            
            t_v.nestedb_.v_i_ = 1;
            
            ser.run(
                ss,
                [&t_v](decltype(ser)& ser, Context& _rctx){
                    ser.add(t_v, _rctx, "test");
                },
                ctx);
        }
        {
            using namespace last;
            Context         ctx;
            DeserializerT   des   = typemap.createDeserializer();
            
            Test t;
            
            des.run(
                ss,
                [&t](decltype(des)& des, Context& _rctx){
                    des.add(t, _rctx, "test");
                },
                ctx);
            solid_check(t.check(t_v));
        }
    }
    
    return 0;
}
