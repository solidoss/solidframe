#include "solid/serialization/serialization.hpp"
#include "solid/serialization/v2/typetraits.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/any.hpp"
#include "solid/utility/function.hpp"
#include <bitset>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;
using namespace solid;

struct A {
    int32_t  a;
    string   s;
    uint64_t b;

    bool operator==(const A& _ra) const
    {
        return a == _ra.a && b == _ra.b && s == _ra.s;
    }
};

template <class S, class Ctx>
void solidSerializeV2(S& _rs, const A& _r, Ctx& _rctx, const char* _name)
{
    _rs.add(_r.a, _rctx, "A.a").add(_r.s, _rctx, "A.s").add(_r.b, _rctx, "A.b");
}

template <class S, class Ctx>
void solidSerializeV2(S& _rs, A& _r, Ctx& _rctx, const char* _name)
{
    _rs.add(_r.a, _rctx, "A.a").add(_r.s, _rctx, "A.s").add(_r.b, _rctx, "A.b");
}

struct Context {
    string output_file_path;
};

class Test {
    string                   p;
    bool                     b;
    vector<A>                v;
    deque<A>                 d;
    map<string, A>           m;
    set<string>              s;
    unordered_map<string, A> um;
    unordered_set<string>    us;
    A                        a;
    vector<bool>             vb;
    bitset<20>               bs;

    std::ostringstream oss;

    void populate(bool _b)
    {
        b = _b;
        if (_b) {
            for (size_t i = 0; i < 100; ++i) {
                v.emplace_back();
                v.back().a = i;
                v.back().b = 100 - i;
                v.back().s = to_string(v.back().a) + " - " + to_string(v.back().b);
                a.s += v.back().s;
                a.s += ' ';
            }
        } else {
            for (size_t i = 0; i < 100; ++i) {
                d.emplace_back();
                d.back().a = i;
                d.back().b = 100 - i;
                d.back().s = to_string(d.back().a) + " - " + to_string(d.back().b);
                a.s += d.back().s;
                a.s += ' ';
            }
        }
        for (size_t i = 0; i < 10; ++i) {
            A a;
            a.a = i;
            a.b = 10 - i;
            a.s = to_string(a.a) + ' ' + to_string(a.b);
            s.insert(a.s);
            us.insert(a.s);

            m[to_string(i)]  = a;
            um[to_string(i)] = std::move(a);
        }
        for (size_t i = 0; i < 20; ++i) {
            vb.push_back((i % 2) == 0);
            bs[i] = ((i % 2) == 0);
        }
    }

public:
    Test() {}

    Test(bool _b, const string& _path)
        : p(_path)
    {
        populate(_b);
    }

    Test(bool _b)
    {
        populate(_b);
    }

    bool operator==(const Test& _rt) const
    {
        std::ifstream     t(p);
        std::stringstream buffer;
        buffer << t.rdbuf();
        string s1 = buffer.str();
        string s2 = _rt.oss.str();
        SOLID_ASSERT(b == _rt.b);
        SOLID_ASSERT(v == _rt.v);
        SOLID_ASSERT(d == _rt.d);
        SOLID_ASSERT(s1 == s2);
        SOLID_ASSERT(m == _rt.m);
        SOLID_ASSERT(s == _rt.s);
        SOLID_ASSERT(um == _rt.um);
        SOLID_ASSERT(us == _rt.us);
        SOLID_ASSERT(vb == _rt.vb);
        SOLID_ASSERT(bs == _rt.bs);
        return b == _rt.b && a == _rt.a && v == _rt.v && d == _rt.d && s1 == s2 && m == _rt.m && s == _rt.s && um == _rt.um && us == _rt.us && vb == _rt.vb && bs == _rt.bs;
    }

    template <class S>
    void solidSerializeV2(S& _rs, Context& _rctx, const char* _name) const
    {

        _rs
            .add(b, _rctx, "b")
            .add(
                [this](S& _rs, Context& _rctx, const char* _name) {
                    if (this->b) {
                        _rs.add(v, _rctx, "v");
                    } else {
                        _rs.add(d, _rctx, "d");
                    }
                },
                _rctx, "f");

        std::ifstream ifs;
        ifs.open(p);
        _rs
            .push(
                [ this, ifs = std::move(ifs) ](S & _rs, Context & _rctx, const char* _name) mutable {
                    _rs.add(ifs, [](std::istream& _ris, uint64_t _len, const bool _done, Context& _rctx, const char* _name) {
                        idbg("Progress(" << _name << "): " << _len << " done = " << _done);
                    },
                        _rctx, _name);
                    return true;
                },
                _rctx, "s")
            .add(m, _rctx, "m")
            .add(s, _rctx, "s")
            .add(um, _rctx, "um")
            .add(us, _rctx, "us")
            .add(a, _rctx, "a");
        _rs.add(vb, _rctx, "vb");
        _rs.add(bs, _rctx, "bs");
        //_rs.add(b, "b").add(v, "v").add(a, "a");
    }

    template <class S>
    void solidSerializeV2(S& _rs, Context& _rctx, const char* _name)
    {
        _rs
            .add(b, _rctx, "b")
            .add(
                [this](S& _rs, Context& _rctx, const char* _name) {
                    if (this->b) {
                        _rs.add(v, _rctx, "v");
                    } else {
                        _rs.add(d, _rctx, "d");
                    }
                },
                _rctx, "f")
            .push(
                if_then_else<S::is_serializer>(
                    [](S& _rs, Context& _rctx, const char* _name) mutable {
                        return true;
                    },
                    [this](S& _rs, Context& _rctx, const char* _name) {
                        _rs.add(oss, [](std::ostream& _ros, uint64_t _len, const bool _done, Context& _rctx, const char* _name) {
                            idbg("Progress(" << _name << "): " << _len << " done = " << _done);
                        },
                            _rctx, _name);
                        return true;
                    }),
                _rctx, "s")
            .add(m, _rctx, "m")
            .add(s, _rctx, "s")
            .add(um, _rctx, "um")
            .add(us, _rctx, "us")
            .add(a, _rctx, "a");
        _rs.add(vb, _rctx, "vb");
        _rs.add(bs, _rctx, "bs");
        //_rs.add(b, _rctx, "b").add(v, _rctx, "v").add(a, _rctx, "a");
    }
};

int test_binary(int argc, char* argv[])
{
#ifdef SOLID_HAS_DEBUG
    Debug::the().levelMask("view");
    Debug::the().moduleMask("any");
    Debug::the().initStdErr(false, nullptr);
#endif

    std::string input_file_path;
    std::string output_file_path;
    {
        std::ifstream ifs;
        Any<>         a{std::move(ifs)};
        a.cast<ifstream>()->open("test.txt");
    }

    {
        std::ifstream ifs;
        auto          lambda = [ifs = std::move(ifs)]() mutable
        {
            ifs.open("test.txt");
        };
        Function<128, void()> f{std::move(lambda)};
        f();
    }

    if (argc > 1) {
        input_file_path = argv[1];
        size_t pos      = input_file_path.rfind('/');
        if (pos == string::npos) {
            pos = 0;
        }
        output_file_path = "./";
        output_file_path += input_file_path.substr(pos + 1);
        output_file_path += ".copy";
    }

    using TypeMapT = serialization::TypeMap<uint8_t, Context, serialization::binary::Serializer, serialization::binary::Deserializer, uint64_t>;

    TypeMapT tm;

    tm.null(0);
    tm.registerType<Test>(1);

    {
        const Test                  t{true, input_file_path};
        const std::shared_ptr<Test> tp{std::make_shared<Test>(true)};
        const std::unique_ptr<Test> tup{new Test(true)};
        std::shared_ptr<Test>       sp1;
        std::unique_ptr<Test>       up1;
        Context                     ctx;
        ostringstream               oss;

        {
            typename TypeMapT::SerializerT ser = tm.createSerializer();

            ser.run(
                oss,
                [&t, &tp, &tup, &sp1, &up1](decltype(ser)& ser, Context& _rctx) {
                    ser.add(t, _rctx, "t").add(tp, _rctx, "tp").add(tup, _rctx, "tup").add(sp1, _rctx, "sp1").add(up1, _rctx, "up1");
                },
                ctx);
        }
        {
            idbg("oss.str.size = " << oss.str().size());
            istringstream                    iss(oss.str());
            typename TypeMapT::DeserializerT des = tm.createDeserializer();

            Test                  t_c;
            std::shared_ptr<Test> tp_c;
            std::unique_ptr<Test> tup_c;
            std::shared_ptr<Test> sp1_c;
            std::unique_ptr<Test> up1_c;

            ctx.output_file_path = output_file_path;

            des.run(iss,
                [&t_c, &tp_c, &tup_c, &sp1_c, &up1_c](decltype(des)& des, Context& ctx) {
                    des.add(t_c, ctx, "t").add(tp_c, ctx, "tp_c").add(tup_c, ctx, "tup_c").add(sp1_c, ctx, "sp1").add(up1_c, ctx, "up1");
                },
                ctx);

            //iss >> des.wrap(ctx);
            SOLID_CHECK(!des.error(), "check failed: " << des.error().message());
            SOLID_CHECK(t == t_c, "check failed");
            SOLID_CHECK(*tp == *tp_c, "check failed");
            SOLID_CHECK(*tup == *tup_c, "check failed");
            SOLID_CHECK(!sp1_c, "check failed");
            SOLID_CHECK(!up1_c, "check failed");
        }
    }
    return 0;
}
