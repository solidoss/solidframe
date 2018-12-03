#include "solid/serialization/v2/serialization.hpp"
#include <bitset>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <vector>

#undef max

using namespace solid;
using namespace std;

namespace {

struct Context {
};

struct TypeData {
};

struct Test;
using TestPointerT = shared_ptr<Test>;

struct Test {
    using KeyValueVectorT = std::vector<std::pair<std::string, std::string>>;
    using MapT            = std::map<std::string, uint64_t>;
    using MapBoolT        = std::map<std::string, bool>;
    using SetT            = std::set<std::string>;

    Test()
        : b(false)
        , sa_sz(0)
        , u8a_sz(0)
    {
    }

    bool            b;
    std::string     str;
    KeyValueVectorT kv_vec;
    MapT            kv_map;
    MapBoolT        kb_map;
    uint32_t        v32;
    deque<bool>     bool_deq;
    bitset<5>       bs5;
    bitset<10>      bs10;
    bitset<20>      bs20;
    bitset<50>      bs50;
    bitset<100>     bs100;
    bitset<1000>    bs1000;
    vector<bool>    bv5;
    vector<bool>    bv10;
    vector<bool>    bv20;
    vector<bool>    bv50;
    vector<bool>    bv100;
    vector<bool>    bv1000;
    SetT            ss;
    std::string     sa[256];
    size_t          sa_sz;
    uint8_t         u8a[512];
    size_t          u8a_sz;

    SOLID_SERIALIZE_CONTEXT_V2(_s, _rthis, _rctx, /*_name*/)
    {
        _s.add(_rthis.str, _rctx, "Test::str");
        _s.add(_rthis.b, _rctx, "Test::b");
        _s.add(_rthis.kv_vec, _rctx, "Test::kv_vec");
        _s.add(_rthis.kv_map, _rctx, "Test::kv_map");
        _s.add(_rthis.kb_map, _rctx, "Test::kb_map");
        _s.add(_rthis.bool_deq, _rctx, "bool_deq");
        _s.add(_rthis.v32, _rctx, "Test::v32");
        _s.add(_rthis.bs5, _rctx, "bs5");
        _s.add(_rthis.bs10, _rctx, "bs10");
        _s.add(_rthis.bs20, _rctx, "bs20");
        _s.add(_rthis.bs50, _rctx, "bs50");
        _s.add(_rthis.bs100, _rctx, "bs100");
        _s.add(_rthis.bs1000, _rctx, "bs1000");
        _s.add(_rthis.bv5, _rctx, "bv5");
        _s.add(_rthis.bv10, _rctx, "bv10");
        _s.add(_rthis.bv20, _rctx, "bv20");
        _s.add(_rthis.bv50, _rctx, "bv50");
        _s.add(_rthis.bv100, _rctx, "bv100");
        _s.add(_rthis.bv1000, _rctx, "bv1000");
        _s.add(_rthis.ss, _rctx, "ss");
        //_s.add(_rthis.sa, _rthis.sa_sz, 256, "sa");
        //_s.add(_rthis.u8a, _rthis.u8a_sz, 512, "u8a");
    }

    void init();
    void check() const;

    static TestPointerT create()
    {
        return make_shared<Test>();
    }
};

} //namespace

int test_container(int /*argc*/, char* /*argv*/[])
{

    solid::log_start(std::cerr, {".*:VIEW"});

    using TypeMapT      = serialization::TypeMap<uint8_t, Context, serialization::binary::Serializer, serialization::binary::Deserializer, TypeData>;
    using SerializerT   = TypeMapT::SerializerT;
    using DeserializerT = TypeMapT::DeserializerT;

    string   test_data;
    TypeMapT typemap;

    typemap.null(0);
    typemap.registerType<Test>(1);

    { //serialization
        Context     ctx;
        SerializerT ser   = typemap.createSerializer();
        const int   bufcp = 64;
        char        buf[bufcp];
        long        rv;

        shared_ptr<Test> test = Test::create();

        test->init();

        rv = ser.run(buf, bufcp, [&test](decltype(ser)& ser, Context& _rctx) { ser.add(test, _rctx, "test"); }, ctx);

        while (rv > 0) {
            test_data.append(buf, rv);
            rv = ser.run(buf, bufcp, ctx);
        }
    }
    { //deserialization
        Context       ctx;
        DeserializerT des = typemap.createDeserializer();

        shared_ptr<Test> test;

        long rv = des.run(test_data.data(), test_data.size(), [&test](decltype(des)& des, Context& _rctx) { des.add(test, _rctx, "test"); }, ctx);

        solid_check(rv == static_cast<long>(test_data.size()));
        test->check();
    }
    return 0;
}

namespace {
const pair<string, string> kv_array[] = {
    {"first_key", "first_value"},
    {"second_key", "secon_value"},
    {"third_key", "third_value"},
    {"fourth_key", "fourth_value"},
    {"fifth_key", "fifth_value"},
    {"sixth_key", "sixth_value"},
    {"seventh_key", "seventh_value"},
    {"eighth_key", "eighth_value"},
    {"nineth_key", "nineth_value"},
    {"tenth_key", "tenth_value"},
};

const size_t kv_array_size = sizeof(kv_array) / sizeof(pair<string, string>);

void Test::init()
{
    kv_vec.reserve(kv_array_size);
    b = true;
    for (size_t i = 0; i < kv_array_size; ++i) {
        str.append(kv_array[i].first);
        str.append(kv_array[i].second);
        kv_vec.push_back(kv_array[i]);
        bool_deq.push_back(((i % 2) == 0));
        kv_map[kv_array[i].first] = i;
        kb_map[kv_array[i].first] = ((i % 2) == 0);
        ss.insert(kv_array[i].first);
    }

    bs5.reset();
    bv5.resize(5, false);
    for (size_t i = 0; i < bs5.size(); ++i) {
        if ((i % 2) == 0) {
            bs5.set(i);
            bv5[i] = true;
        }
    }

    bs10.reset();
    bv10.resize(10, false);
    for (size_t i = 0; i < bs10.size(); ++i) {
        if ((i % 2) == 0) {
            bs10.set(i);
            bv10[i] = true;
        }
    }

    bs20.reset();
    bv20.resize(20, false);
    for (size_t i = 0; i < bs20.size(); ++i) {
        if ((i % 2) == 0) {
            bs20.set(i);
            bv20[i] = true;
        }
    }

    bs50.reset();
    bv50.resize(50, false);
    for (size_t i = 0; i < bs50.size(); ++i) {
        if ((i % 2) == 0) {
            bs50.set(i);
            bv50[i] = true;
        }
    }

    bs100.reset();
    bv100.resize(100, false);
    for (size_t i = 0; i < bs100.size(); ++i) {
        if ((i % 2) == 0) {
            bs100.set(i);
            bv100[i] = true;
        }
    }

    bs1000.reset();
    bv1000.resize(1000, false);
    for (size_t i = 0; i < bs1000.size(); ++i) {
        if ((i % 2) == 0) {
            bs1000.set(i);
            bv1000[i] = true;
        }
    }

    v32 = str.size();

    for (size_t i = 0; i < 100; ++i) {
        sa[i] = kv_array[i % kv_array_size].second;
    }
    sa_sz = 100;

    for (size_t i = 0; i < 500; ++i) {
        u8a[i] = i % std::numeric_limits<uint8_t>::max();
    }
    u8a_sz = 500;

    check();
}

void Test::check() const
{
    solid_dbg(generic_logger, Info, "str = " << str);
    solid_dbg(generic_logger, Info, "bs5 = " << bs5.to_string());
    solid_dbg(generic_logger, Info, "bs10 = " << bs10.to_string());
    solid_dbg(generic_logger, Info, "bs20 = " << bs20.to_string());
    solid_dbg(generic_logger, Info, "bs50 = " << bs50.to_string());
    solid_dbg(generic_logger, Info, "bs100 = " << bs100.to_string());
    solid_dbg(generic_logger, Info, "bs1000 = " << bs1000.to_string());
    solid_check(b);
    solid_check(kv_vec.size() == kv_map.size());
    string tmpstr;
    for (size_t i = 0; i < kv_vec.size(); ++i) {
        tmpstr.append(kv_array[i].first);
        tmpstr.append(kv_array[i].second);
        solid_check(kv_vec[i] == kv_array[i]);
        solid_check(bool_deq[i] == ((i % 2) == 0));
        MapT::const_iterator it = kv_map.find(kv_array[i].first);
        solid_check(it != kv_map.end());
        solid_check(it->second == i);
        MapBoolT::const_iterator itb = kb_map.find(kv_array[i].first);
        solid_check(itb != kb_map.end());
        solid_check(itb->second == ((i % 2) == 0));

        SetT::const_iterator ssit = ss.find(kv_array[i].first);
        solid_check(ssit != ss.end());
    }
    solid_check(tmpstr == str);
    solid_check(str.size() == v32);

    for (size_t i = 0; i < bs5.size(); ++i) {
        if ((i % 2) == 0) {
            solid_check(bs5[i]);
            solid_check(bv5[i]);
        } else {
            solid_check(!bs5[i]);
            solid_check(!bv5[i]);
        }
    }

    for (size_t i = 0; i < bs10.size(); ++i) {
        if ((i % 2) == 0) {
            solid_check(bs10[i]);
            solid_check(bv10[i]);
        } else {
            solid_check(!bs10[i]);
            solid_check(!bv10[i]);
        }
    }

    for (size_t i = 0; i < bs20.size(); ++i) {
        if ((i % 2) == 0) {
            solid_check(bs20[i]);
            solid_check(bv20[i]);
        } else {
            solid_check(!bs20[i]);
            solid_check(!bv20[i]);
        }
    }

    for (size_t i = 0; i < bs50.size(); ++i) {
        if ((i % 2) == 0) {
            solid_check(bs50[i]);
            solid_check(bv50[i]);
        } else {
            solid_check(!bs50[i]);
            solid_check(!bv50[i]);
        }
    }

    for (size_t i = 0; i < bs100.size(); ++i) {
        if ((i % 2) == 0) {
            solid_check(bs100[i]);
            solid_check(bv100[i]);
        } else {
            solid_check(!bs100[i]);
            solid_check(!bv100[i]);
        }
    }

    for (size_t i = 0; i < bs1000.size(); ++i) {
        if ((i % 2) == 0) {
            solid_check(bs1000[i]);
            solid_check(bv1000[i]);
        } else {
            solid_check(!bs1000[i]);
            solid_check(!bv1000[i]);
        }
    }
#if 0
    solid_check(sa_sz == 100);
    for (size_t i = 0; i < sa_sz; ++i) {
        solid_check(sa[i] == kv_array[i % kv_array_size].second);
    }

    solid_check(u8a_sz == 500);
    for (size_t i = 0; i < u8a_sz; ++i) {
        solid_check(u8a[i] == (i % std::numeric_limits<uint8_t>::max()));
    }
#endif
}

} //namespace
