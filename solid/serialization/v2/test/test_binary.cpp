#include "solid/serialization/serialization.hpp"
#include "solid/serialization/v2/typetraits.hpp"
#include "solid/system/exception.hpp"
#include <deque>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

using namespace std;
using namespace solid;

struct A {
    int32_t  a;
    uint64_t b;

    bool operator==(const A& _ra) const
    {
        return a == _ra.a && b == _ra.b;
    }
};

template <class S>
void solidSerializeV2(S& _rs, const A& _r, const char* _name)
{
    _rs.add(_r.a, "a").add(_r.b, "b");
}

template <class S, class Ctx>
void solidSerializeV2(S& _rs, A& _r, Ctx& _rctx, const char* _name)
{
    _rs.add(_r.a, _rctx, "a").add(_r.b, _rctx, "b");
}

struct Context {
};

class Test {
    bool      b;
    vector<A> v;
    deque<A>  d;
    A         a;

    void populate(bool _b)
    {
        b = _b;
        if (_b) {
            for (size_t i = 0; i < 100; ++i) {
                v.emplace_back();
                v.back().a = i;
                v.back().b = 100 - i;
            }
        } else {
            for (size_t i = 0; i < 100; ++i) {
                d.emplace_back();
                d.back().a = i;
                d.back().b = 100 - i;
            }
        }
    }

public:
    Test() {}

    Test(bool _b)
    {
        populate(_b);
    }

    bool operator==(const Test& _rt) const
    {
        return b == _rt.b && a == _rt.a && v == _rt.v && d == _rt.d;
    }

    template <class S>
    void solidSerializeV2(S& _rs, const char* _name) const
    {
        _rs.add(b, "b").add(
                           [this](S& _rs, const char* _name) {
                               if (this->b) {
                                   _rs.add(v, "v");
                               } else {
                                   _rs.add(d, "d");
                               }
                           },
                           "f")
            .add(a, "a");
        //_rs.add(b, "b").add(v, "v").add(a, "a");
    }

    template <class S>
    void solidSerializeV2(S& _rs, Context& _rctx, const char* _name)
    {
        _rs.add(b, _rctx, "b").add([this](S& _rs, Context& _rctx, const char* _name) {
                                  if (this->b) {
                                      _rs.add(v, _rctx, "v");
                                  } else {
                                      _rs.add(d, _rctx, "d");
                                  }
                              },
                                  _rctx, "f")
            .add(a, _rctx, "a");
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

    {
        const Test                  t{true};
        const std::shared_ptr<Test> tp{std::make_shared<Test>(true)};
        const std::unique_ptr<Test> tup{new Test(true)};

        ostringstream oss;
        {
            serialization::binary::Serializer<> ser;

            ser.add(t, "t").add(tp, "tp").add(tup, "tup");

            oss << ser;
        }
        {
            idbg("oss.str.size = " << oss.str().size());
            istringstream                                iss(oss.str());
            serialization::binary::Deserializer<Context> des;

            Test                  t_c;
            std::shared_ptr<Test> tp_c;
            std::unique_ptr<Test> tup_c;
            Context               ctx;

            des.add(t_c, ctx, "t").add(tp_c, ctx, "tp_c").add(tup_c, ctx, "tup_c");

            iss >> des.wrap(ctx);

            SOLID_CHECK(t == t_c, "check failed");
        }
    }
    return 0;
}
