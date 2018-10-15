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
void solidSerializeV2(S& _rs, const A& _r, Ctx& _rctx, const char* /*_name*/)
{
    _rs.add(_r.a, _rctx, "A.a").add(_r.s, _rctx, "A.s").add(_r.b, _rctx, "A.b");
}

template <class S, class Ctx>
void solidSerializeV2(S& _rs, A& _r, Ctx& _rctx, const char* /*_name*/)
{
    _rs.add(_r.a, _rctx, "A.a").add(_r.s, _rctx, "A.s").add(_r.b, _rctx, "A.b");
}

struct Context {
    string output_file_path;
};

class Test {
    static constexpr size_t BlobCapacity = 40 * 1024;

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
    vector<char>             vc;
    array<A, 10>             a1;
    array<A, 20>             a2;
    uint16_t                 a2_sz;
    uint32_t                 blob_sz;
    char                     blob[BlobCapacity];
    uint32_t                 blob32_sz;
    char                     blob32[sizeof(uint32_t)];
    uint32_t                 blob64_sz;
    char                     blob64[sizeof(uint64_t)];

    std::ostringstream oss;

    void populate(bool _b)
    {
        string blb;
        b   = _b;
        a.a = 540554784UL;
        a.b = 2321664020290347053ULL;
        serialization::binary::store(blob32, static_cast<uint32_t>(a.a));
        {
            uint32_t v;
            solid::serialization::binary::load(blob32, v);
            solid_assert(v == static_cast<uint32_t>(a.a));
        }

        serialization::binary::store(blob64, a.b);
        blob32_sz = sizeof(uint32_t);
        blob64_sz = sizeof(uint64_t);

        if (_b) {
            for (size_t i = 0; i < 100; ++i) {
                v.emplace_back();
                v.back().a = i;
                v.back().b = 100 - i;
                v.back().s = to_string(v.back().a) + " - " + to_string(v.back().b);
                a.s += v.back().s;
                a.s += ' ';
                blb += a.s;
            }
        } else {
            for (size_t i = 0; i < 100; ++i) {
                d.emplace_back();
                d.back().a = i;
                d.back().b = 100 - i;
                d.back().s = to_string(d.back().a) + " - " + to_string(d.back().b);
                a.s += d.back().s;
                a.s += ' ';
                blb += a.s;
            }
        }
        for (size_t i = 0; i < 10; ++i) {
            A a;
            a.a = i;
            a.b = 10 - i;
            a.s = to_string(a.a) + ' ' + to_string(a.b);
            blb += a.s;
            s.insert(a.s);
            us.insert(a.s);

            m[to_string(i)]  = a;
            a1[i]            = a;
            a2[i]            = a;
            um[to_string(i)] = std::move(a);
        }
        a2_sz = 10;
        for (size_t i = 0; i < 20; ++i) {
            vb.push_back((i % 2) == 0);
            bs[i] = ((i % 2) == 0);
            vc.push_back('a' + static_cast<char>(i));
        }

        solid_assert(blb.size() < BlobCapacity);
        memcpy(blob, blb.data(), blb.size());
        blob_sz = blb.size();
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
        solid_assert(b == _rt.b);
        solid_assert(v == _rt.v);
        solid_assert(d == _rt.d);
        solid_assert(s1 == s2);
        solid_assert(m == _rt.m);
        solid_assert(s == _rt.s);
        solid_assert(um == _rt.um);
        solid_assert(us == _rt.us);
        solid_assert(vb == _rt.vb);
        solid_assert(bs == _rt.bs);
        solid_assert(vc == _rt.vc);
        solid_assert(a1 == _rt.a1);
        solid_assert(a2_sz == _rt.a2_sz);
        for (decltype(a2_sz) i = 0; i < a2_sz; ++i) {
            if (a2[i] == _rt.a2[i]) {

            } else {
                solid_assert(false);
                return false;
            }
        }
        solid_assert(blob_sz == _rt.blob_sz);
        solid_assert(memcmp(blob, _rt.blob, blob_sz) == 0);
        solid_assert(blob32_sz == _rt.blob32_sz);
        solid_assert(memcmp(blob32, _rt.blob32, blob32_sz) == 0);
        solid_assert(blob64_sz == _rt.blob64_sz);
        solid_assert(memcmp(blob64, _rt.blob64, blob64_sz) == 0);
        {
            uint32_t v;
            solid::serialization::binary::load(blob32, v);
            solid_assert(v == static_cast<uint32_t>(a.a));
        }
        {
            uint64_t v;
            solid::serialization::binary::load(blob64, v);
            solid_assert(v == a.b);
        }

        return b == _rt.b && a == _rt.a && v == _rt.v && d == _rt.d && s1 == s2 && m == _rt.m && s == _rt.s && um == _rt.um && us == _rt.us && vb == _rt.vb && bs == _rt.bs && vc == _rt.vc;
    }

    template <class S>
    void solidSerializeV2(S& _rs, Context& _rctx, const char* /*_name*/) const
    {
        _rs
            .add(b, _rctx, "b")
            .add(
                [this](S& _rs, Context& _rctx, const char* /*_name*/) {
                    if (this->b) {
                        _rs.add(v, _rctx, "v");
                    } else {
                        _rs.add(d, _rctx, "d");
                    }
                },
                _rctx, "f");

        using IFStreamPtrT = std::unique_ptr<std::ifstream>;

        IFStreamPtrT pifs(new ifstream);
        pifs->open(p);
        _rs
            .push(
                [pifs = std::move(pifs)](S& _rs, Context& _rctx, const char* _name) mutable {
                    _rs.add(*pifs, [](std::istream& _ris, uint64_t _len, const bool _done, Context& _rctx, const char* _name) {
                        solid_dbg(generic_logger, Info, "Progress(" << _name << "): " << _len << " done = " << _done);
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
        _rs.add(vc, _rctx, "vc");
        _rs.add(a1, _rctx, "a1");
        _rs.add(a2, a2_sz, _rctx, "a2");
        _rs.add(blob, blob_sz, BlobCapacity, _rctx, "blob");
        _rs.add(blob32, blob32_sz, sizeof(uint32_t), _rctx, "blob32");
        _rs.add(blob64, blob64_sz, sizeof(uint64_t), _rctx, "blob64");
        //_rs.add(b, "b").add(v, "v").add(a, "a");
    }

    template <class S>
    void solidSerializeV2(S& _rs, Context& _rctx, const char* /*_name*/)
    {
        _rs
            .add(b, _rctx, "b")
            .add(
                [this](S& _rs, Context& _rctx, const char* /*_name*/) {
                    if (this->b) {
                        _rs.add(v, _rctx, "v");
                    } else {
                        _rs.add(d, _rctx, "d");
                    }
                },
                _rctx, "f")
            .push(
                if_then_else<S::is_serializer>(
                    [](S& _rs, Context& _rctx, const char* /*_name*/) mutable {
                        return true;
                    },
                    [this](S& _rs, Context& _rctx, const char* _name) {
                        _rs.add(oss, [](std::ostream& _ros, uint64_t _len, const bool _done, Context& _rctx, const char* _name) {
                            solid_dbg(generic_logger, Info, "Progress(" << _name << "): " << _len << " done = " << _done);
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
        _rs.add(vc, _rctx, "vc");
        _rs.add(a1, _rctx, "a1");
        _rs.add(a2, a2_sz, _rctx, "a2");
        _rs.add(blob, blob_sz, BlobCapacity, _rctx, "blob");
        _rs.add(blob32, blob32_sz, sizeof(uint32_t), _rctx, "blob32");
        _rs.add(blob64, blob64_sz, sizeof(uint64_t), _rctx, "blob64");
        //_rs.add(b, _rctx, "b").add(v, _rctx, "v").add(a, _rctx, "a");
    }
};

int test_binary(int argc, char* argv[])
{
    solid::log_start(std::cout, {".*:EW"});
    //solid::log_start(argv[0], {".*:VIEW"}, true, 2, 1024 * 1024);

    std::string input_file_path;
    std::string output_file_path;
    char        choice = 'v';
    std::string archive_path;

    {
        using IFStreamPtrT = std::unique_ptr<std::ifstream>;
        IFStreamPtrT pifs(new ifstream);
        Any<>        a{std::move(pifs)};
        (*a.cast<IFStreamPtrT>())->open("test.txt");
    }

    {
        auto lambda = [pifs = std::unique_ptr<std::ifstream>(new ifstream)]() mutable {
            pifs->open("test.txt");
        };
        Function<128, void()> f{std::move(lambda)};
        f();
    }

    if (argc > 1) {
#ifdef SOLID_ON_WINDOWS
        const char path_sep = '\\';
#else
        const char path_sep = '/';
#endif
        input_file_path = argv[1];
        size_t pos      = input_file_path.rfind(path_sep);
        if (pos == string::npos) {
            pos = 0;
        } else {
            ++pos;
        }
        //output_file_path = "./";
        output_file_path += input_file_path.substr(pos);
        output_file_path += ".copy";
    }

    if (argc > 2) {
        choice = argv[2][0];
    }

    if (argc > 3) {
        archive_path = argv[3];
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

        if (choice != 'l') {
            typename TypeMapT::SerializerT ser = tm.createSerializer();
            ofstream                       ofs;

            if (!archive_path.empty()) {
                ofs.open(archive_path, ios_base::out | ios_base::binary);
            }

            ostream& ros = ofs.is_open() ? static_cast<ostream&>(std::ref(ofs)) : static_cast<ostream&>(std::ref(oss));

            ser.run(
                ros,
                [&t, &tp, &tup, &sp1, &up1](decltype(ser)& ser, Context& _rctx) {
                    ser.add(t, _rctx, "t").add(tp, _rctx, "tp").add(tup, _rctx, "tup").add(sp1, _rctx, "sp1").add(up1, _rctx, "up1");
                },
                ctx);
        }

        if (choice != 's') {

            solid_dbg(generic_logger, Info, "oss.str.size = " << oss.str().size());

            ifstream ifs;

            if (!archive_path.empty()) {
                ifs.open(archive_path, ios_base::in | ios_base::binary);
                solid_assert(ifs.is_open());
            }

            istringstream                    iss(oss.str());
            typename TypeMapT::DeserializerT des = tm.createDeserializer();

            istream& ris = ifs.is_open() ? static_cast<istream&>(std::ref(ifs)) : static_cast<istream&>(std::ref(iss));

            Test                  t_c;
            std::shared_ptr<Test> tp_c;
            std::unique_ptr<Test> tup_c;
            std::shared_ptr<Test> sp1_c;
            std::unique_ptr<Test> up1_c;

            ctx.output_file_path = output_file_path;

            des.run(ris,
                [&t_c, &tp_c, &tup_c, &sp1_c, &up1_c](decltype(des)& des, Context& ctx) {
                    des.add(t_c, ctx, "t").add(tp_c, ctx, "tp_c").add(tup_c, ctx, "tup_c").add(sp1_c, ctx, "sp1").add(up1_c, ctx, "up1");
                },
                ctx);

            //iss >> des.wrap(ctx);
            solid_check(!des.error(), "check failed: " << des.error().message());
            solid_check(t == t_c, "check failed");
            solid_check(*tp == *tp_c, "check failed");
            solid_check(*tup == *tup_c, "check failed");
            solid_check(!sp1_c, "check failed");
            solid_check(!up1_c, "check failed");
        }
    }
    return 0;
}
