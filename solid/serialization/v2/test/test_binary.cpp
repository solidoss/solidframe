#include "solid/serialization/serialization.hpp"
#include "solid/serialization/v2/typetraits.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/any.hpp"
#include "solid/utility/function.hpp"
#include <deque>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <type_traits>
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

template <class S>
void solidSerializeV2(S& _rs, const A& _r, const char* _name)
{
    _rs.add(_r.a, "A.a").add(_r.s, "A.s").add(_r.b, "A.b");
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
    string             p;
    bool               b;
    vector<A>          v;
    deque<A>           d;
    A                  a;
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
        return b == _rt.b && a == _rt.a && v == _rt.v && d == _rt.d;
    }

    template <class S>
    void solidSerializeV2(S& _rs, const char* _name) const
    {

        _rs
            .add(b, "b")
            .add(
                [this](S& _rs, const char* _name) {
                    if (this->b) {
                        _rs.add(v, "v");
                    } else {
                        _rs.add(d, "d");
                    }
                },
                "f");

        std::ifstream ifs;
        _rs
            .push(make_choice<S::is_serializer>(
                      [ this, ifs = std::move(ifs) ](S & _rs, const char* _name) mutable {
                          ifs.open(p);
                          //_rs.add(ifs, _name);
                          return true;
                      },
                      [](S& _rs, const char* _name) {
                          return true;
                      }),
                "s")
            .add(a, "a");
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
                make_choice<S::is_serializer>(
                    [](S& _rs, Context& _rctx, const char* _name) mutable {
                        return true;
                    },
                    [this](S& _rs, Context& _rctx, const char* _name) {
                        //_rs.add(oss, _rctx, _name);
                        return true;
                    }),
                _rctx, "s")
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

    {
        const Test                  t{true, input_file_path};
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
            ctx.output_file_path = output_file_path;

            des.add(t_c, ctx, "t").add(tp_c, ctx, "tp_c").add(tup_c, ctx, "tup_c");

            iss >> des.wrap(ctx);

            SOLID_CHECK(t == t_c, "check failed");
        }
    }
    return 0;
}
