// Wire compatibility between serialization v3 (reflection v1) and
// serialization v4 (reflection v2): the same types, reflected for both, must
// - serialize to identical bytes with v3 and v4, for any buffer size, and
// - deserialize with either version from the bytes produced by the other.

#include <array>
#include <bitset>
#include <cstdint>
#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "solid/serialization/v3/serialization.hpp"
#include "solid/serialization/v4/serialization.hpp"
#include "solid/system/exception.hpp"

using namespace std;
using namespace solid;
using namespace std::literals;

namespace {

struct Context {
    uint64_t stream_bytes = 0;
};

using TypeIdT = uint16_t;

using SerializerV3T   = serialization::v3::binary::Serializer<reflection::v1::metadata::Variant<Context>, decltype(reflection::v1::metadata::factory), Context, TypeIdT>;
using DeserializerV3T = serialization::v3::binary::Deserializer<reflection::v1::metadata::Variant<Context>, decltype(reflection::v1::metadata::factory), Context, TypeIdT>;
using SerializerV4T   = serialization::v4::binary::Serializer<Context, TypeIdT>;
using DeserializerV4T = serialization::v4::binary::Deserializer<Context, TypeIdT>;

// v3 runs on a pointer and a size, v4 on a byte span.
template <class Ser, class... Args>
ptrdiff_t run_ser(Ser& _rser, vector<char>& _rbuf, Args&&... _args)
{
    if constexpr (std::is_same_v<Ser, SerializerV3T>) {
        return _rser.run(_rbuf.data(), static_cast<unsigned>(_rbuf.size()), std::forward<Args>(_args)...);
    } else {
        return _rser.run(as_writable_bytes(span{_rbuf}), std::forward<Args>(_args)...);
    }
}

template <class Des, class... Args>
ptrdiff_t run_des(Des& _rdes, string const& _data, const size_t _offset, const size_t _sz, Args&&... _args)
{
    if constexpr (std::is_same_v<Des, DeserializerV3T>) {
        return _rdes.run(_data.data() + _offset, _sz, std::forward<Args>(_args)...);
    } else {
        return _rdes.run(as_bytes(span{_data}).subspan(_offset, _sz), std::forward<Args>(_args)...);
    }
}

struct Sub {
    uint64_t a_ = 0;
    string   b_;
    int32_t  c_ = 0;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.a_, _rctx, 1, "a").add(_rthis.b_, _rctx, 2, "b").add(_rthis.c_, _rctx, 3, "c");
    }

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("a"sv, rthis.a_), _rc);
        _rr(id<2>, make("b"sv, rthis.b_), _rc);
        _rr(id<3>, make("c"sv, rthis.c_), _rc);
    }

    bool operator==(Sub const&) const  = default;
    auto operator<=>(Sub const&) const = default;
};

struct Base {
    uint32_t base_ = 0;

    virtual ~Base() {}
    virtual bool equals(Base const& _o) const = 0;
};

struct Derived1 : Base {
    string      text_;
    vector<Sub> subs_;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.base_, _rctx, 1, "base").add(_rthis.text_, _rctx, 2, "text").add(_rthis.subs_, _rctx, 3, "subs");
    }

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("base"sv, rthis.base_), _rc);
        _rr(id<2>, make("text"sv, rthis.text_), _rc);
        _rr(id<3>, make("subs"sv, rthis.subs_), _rc);
    }

    bool equals(Base const& _o) const override
    {
        auto const* po = dynamic_cast<Derived1 const*>(&_o);
        return po != nullptr && base_ == po->base_ && text_ == po->text_ && subs_ == po->subs_;
    }
};

struct Derived2 : Base {
    map<string, uint32_t> values_;
    shared_ptr<Base>      next_;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.base_, _rctx, 1, "base").add(_rthis.values_, _rctx, 2, "values").add(_rthis.next_, _rctx, 3, "next");
    }

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("base"sv, rthis.base_), _rc);
        _rr(id<2>, make("values"sv, rthis.values_), _rc);
        _rr(id<3>, make("next"sv, rthis.next_), _rc);
    }

    bool equals(Base const& _o) const override
    {
        auto const* po = dynamic_cast<Derived2 const*>(&_o);
        return po != nullptr && base_ == po->base_ && values_ == po->values_ && ((!next_ && !po->next_) || (next_ && po->next_ && next_->equals(*po->next_)));
    }
};

bool base_equal(shared_ptr<Base> const& _p1, shared_ptr<Base> const& _p2)
{
    return (!_p1 && !_p2) || (_p1 && _p2 && _p1->equals(*_p2));
}

struct Message {
    bool                     b_   = false;
    int8_t                   i8_  = 0;
    uint8_t                  u8_  = 0;
    int16_t                  i16_ = 0;
    uint16_t                 u16_ = 0;
    int32_t                  i32_ = 0;
    uint32_t                 u32_ = 0;
    int64_t                  i64_ = 0;
    uint64_t                 u64_ = 0;
    char                     c_   = 0;
    string                   s_;
    vector<char>             vc_;
    vector<uint8_t>          vu8_;
    vector<uint32_t>         vu32_;
    vector<Sub>              vsub_;
    deque<Sub>               dsub_;
    list<Sub>                lsub_;
    map<string, Sub>         msub_;
    set<string>              sset_;
    map<uint32_t, bool>      mb_;
    vector<bool>             vb_;
    deque<bool>              db_;
    bitset<20>               bs20_;
    bitset<100>              bs100_;
    array<uint8_t, 10>       a8_{};
    array<bool, 4>           abool_{};
    uint16_t                 asub_cnt_ = 0;
    array<Sub, 5>            asub_{};
    pair<uint32_t, string>   pair_;
    tuple<int16_t, string>   tuple_;
    Sub                      sub_;
    shared_ptr<Base>         sbase_;
    unique_ptr<Base>         ubase_;
    shared_ptr<Base>         null_;
    vector<shared_ptr<Base>> vbase_;
    mutable istringstream    iss_; // serialized from
    ostringstream            oss_; // deserialized into
    uint64_t                 tail_ = 0;
    // compacted: v3 reflection::compacted{} vs v4 with_compacted()
    uint8_t  cu8_  = 0;
    uint16_t cu16_ = 0;
    uint32_t cu32_ = 0;
    uint64_t cu64_ = 0;
    int64_t  ci64_ = 0;
    int32_t  ci32_ = 0;
    int8_t   ci8_  = 0;

    void populate(const size_t _seed, const size_t _stream_size)
    {
        b_   = (_seed % 2) == 0;
        i8_  = static_cast<int8_t>(-1 - static_cast<int>(_seed));
        u8_  = static_cast<uint8_t>(200 + _seed);
        i16_ = static_cast<int16_t>(-30000 + static_cast<int>(_seed));
        u16_ = static_cast<uint16_t>(60000 + _seed);
        i32_ = -2000000000 + static_cast<int32_t>(_seed);
        u32_ = 4000000000u + static_cast<uint32_t>(_seed);
        i64_ = -9000000000000000000LL + static_cast<int64_t>(_seed);
        u64_ = 18000000000000000000ULL + _seed;
        c_   = static_cast<char>('a' + _seed);
        for (size_t i = 0; i < 200 + _seed * 50; ++i) {
            s_ += static_cast<char>('A' + (i % 26));
        }
        for (size_t i = 0; i < 40; ++i) {
            vc_.push_back(static_cast<char>(i));
            vu8_.push_back(static_cast<uint8_t>(255 - i));
            vu32_.push_back(static_cast<uint32_t>(i * 0x01020304u));
        }
        for (size_t i = 0; i < 12 + _seed; ++i) {
            Sub sub{.a_ = i * 1000003ULL + _seed, .b_ = "sub_" + std::to_string(i), .c_ = -static_cast<int32_t>(i)};
            vsub_.push_back(sub);
            dsub_.push_front(sub);
            lsub_.push_back(sub);
            msub_["k" + std::to_string(i)] = sub;
            sset_.insert(sub.b_ + "_set");
            mb_[static_cast<uint32_t>(i * 13)] = (i % 3) == 0;
        }
        for (size_t i = 0; i < 70 + _seed; ++i) {
            vb_.push_back((i % 5) < 2);
            db_.push_back((i % 2) == 1);
        }
        for (size_t i = 0; i < bs20_.size(); ++i) {
            bs20_[i] = (i % 4) == 0;
        }
        for (size_t i = 0; i < bs100_.size(); ++i) {
            bs100_[i] = (i % 3) == 2;
        }
        for (size_t i = 0; i < a8_.size(); ++i) {
            a8_[i] = static_cast<uint8_t>(i * 17 + _seed);
        }
        abool_    = {true, false, false, true};
        asub_cnt_ = 3;
        for (size_t i = 0; i < asub_.size(); ++i) {
            asub_[i] = Sub{i, "arr_" + std::to_string(i), static_cast<int32_t>(i)};
        }
        pair_  = {0xABCDEF01u, "pair"};
        tuple_ = {-321, "tuple"};
        sub_   = Sub{77, "nested", -77};

        auto d1   = std::make_shared<Derived1>();
        d1->base_ = 1;
        d1->text_ = "derived one";
        d1->subs_ = {Sub{1, "x", 2}, Sub{3, "y", 4}};
        sbase_    = d1;

        auto d2     = std::make_unique<Derived2>();
        d2->base_   = 2;
        d2->values_ = {{"one", 1}, {"two", 2}};
        auto d1n    = std::make_shared<Derived1>();
        d1n->base_  = 3;
        d1n->text_  = "chained";
        d2->next_   = d1n;
        ubase_      = std::move(d2);
        vbase_      = {sbase_, nullptr, d1n};

        string stream_data;
        for (size_t i = 0; i < _stream_size; ++i) {
            stream_data += static_cast<char>('a' + (i * 7 + _seed) % 26);
        }
        iss_.str(stream_data);
        tail_ = 0x0102030405060708ULL;
        cu8_  = static_cast<uint8_t>(200 + _seed);
        cu16_ = _seed == 0 ? uint16_t{0xFFFF} : static_cast<uint16_t>(5 + _seed); // all bits set: the marker
        cu32_ = _seed == 1 ? 0xFFFFFFFFu : 4000000000u + static_cast<uint32_t>(_seed);
        cu64_ = std::numeric_limits<uint64_t>::max() - _seed;
        ci64_ = -1 - static_cast<int64_t>(_seed); // seed 0: the UINT64_MAX marker
        ci32_ = -2 - static_cast<int32_t>(_seed); // sign extended: wider than the field
        ci8_  = static_cast<int8_t>(-1 + 100 * _seed); // -1, 99, -57
    }

    bool operator==(Message const& _o) const
    {
        bool asub_eq = asub_cnt_ == _o.asub_cnt_;
        for (size_t i = 0; asub_eq && i < asub_cnt_; ++i) {
            asub_eq = asub_[i] == _o.asub_[i];
        }
        bool vbase_eq = vbase_.size() == _o.vbase_.size();
        for (size_t i = 0; vbase_eq && i < vbase_.size(); ++i) {
            vbase_eq = base_equal(vbase_[i], _o.vbase_[i]);
        }
        return b_ == _o.b_ && i8_ == _o.i8_ && u8_ == _o.u8_ && i16_ == _o.i16_ && u16_ == _o.u16_ && i32_ == _o.i32_ && u32_ == _o.u32_
            && i64_ == _o.i64_ && u64_ == _o.u64_ && c_ == _o.c_ && s_ == _o.s_ && vc_ == _o.vc_ && vu8_ == _o.vu8_ && vu32_ == _o.vu32_
            && vsub_ == _o.vsub_ && dsub_ == _o.dsub_ && lsub_ == _o.lsub_ && msub_ == _o.msub_ && sset_ == _o.sset_ && mb_ == _o.mb_
            && vb_ == _o.vb_ && db_ == _o.db_ && bs20_ == _o.bs20_ && bs100_ == _o.bs100_ && a8_ == _o.a8_ && abool_ == _o.abool_
            && asub_eq && pair_ == _o.pair_ && tuple_ == _o.tuple_ && sub_ == _o.sub_ && base_equal(sbase_, _o.sbase_)
            && ((!ubase_ && !_o.ubase_) || (ubase_ && _o.ubase_ && ubase_->equals(*_o.ubase_))) && !_o.null_ && vbase_eq
            && iss_.str() == _o.oss_.str() && tail_ == _o.tail_
            && cu8_ == _o.cu8_ && cu16_ == _o.cu16_ && cu32_ == _o.cu32_ && cu64_ == _o.cu64_ && ci64_ == _o.ci64_
            && ci32_ == _o.ci32_ && ci8_ == _o.ci8_;
    }

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.b_, _rctx, 1, "b").add(_rthis.i8_, _rctx, 2, "i8").add(_rthis.u8_, _rctx, 3, "u8");
        _rr.add(_rthis.i16_, _rctx, 4, "i16").add(_rthis.u16_, _rctx, 5, "u16").add(_rthis.i32_, _rctx, 6, "i32");
        _rr.add(_rthis.u32_, _rctx, 7, "u32").add(_rthis.i64_, _rctx, 8, "i64").add(_rthis.u64_, _rctx, 9, "u64");
        _rr.add(_rthis.c_, _rctx, 10, "c");
        _rr.add(_rthis.s_, _rctx, 11, "s", [](auto& _rmeta) { _rmeta.maxSize(10000); });
        _rr.add(_rthis.vc_, _rctx, 12, "vc").add(_rthis.vu8_, _rctx, 13, "vu8").add(_rthis.vu32_, _rctx, 14, "vu32");
        _rr.add(_rthis.vsub_, _rctx, 15, "vsub", [](auto& _rmeta) { _rmeta.maxSize(100); });
        _rr.add(_rthis.dsub_, _rctx, 16, "dsub").add(_rthis.lsub_, _rctx, 17, "lsub").add(_rthis.msub_, _rctx, 18, "msub");
        _rr.add(_rthis.sset_, _rctx, 19, "sset").add(_rthis.mb_, _rctx, 20, "mb");
        _rr.add(_rthis.vb_, _rctx, 21, "vb").add(_rthis.db_, _rctx, 22, "db");
        _rr.add(_rthis.bs20_, _rctx, 23, "bs20").add(_rthis.bs100_, _rctx, 24, "bs100");
        _rr.add(_rthis.a8_, _rctx, 25, "a8").add(_rthis.abool_, _rctx, 26, "abool");
        _rr.add(_rthis.asub_cnt_, _rctx, 27, "asub_cnt");
        _rr.add(_rthis.asub_, _rctx, 28, "asub", [&_rthis](auto& _rmeta) { _rmeta.size(_rthis.asub_cnt_); });
        _rr.add(_rthis.pair_, _rctx, 29, "pair").add(_rthis.tuple_, _rctx, 30, "tuple").add(_rthis.sub_, _rctx, 31, "sub");
        _rr.add(_rthis.sbase_, _rctx, 32, "sbase").add(_rthis.ubase_, _rctx, 33, "ubase").add(_rthis.null_, _rctx, 34, "null");
        _rr.add(_rthis.vbase_, _rctx, 35, "vbase");
        if constexpr (Reflector::is_const_reflector) {
            _rr.add(_rthis.iss_, _rctx, 36, "stream", [](auto& _rmeta) {
                _rmeta.progressFunction([](Context&, std::istream&, uint64_t, bool, size_t, const char*) {});
            });
        } else {
            _rr.add(_rthis.oss_, _rctx, 36, "stream", [](auto& _rmeta) {
                _rmeta.progressFunction([](Context&, std::ostream&, uint64_t, bool, size_t, const char*) {});
            });
        }
        _rr.add(_rthis.tail_, _rctx, 37, "tail");
        _rr.add(reflection::v1::compacted{_rthis.cu8_}, _rctx, 38, "cu8").add(reflection::v1::compacted{_rthis.cu16_}, _rctx, 39, "cu16");
        _rr.add(reflection::v1::compacted{_rthis.cu32_}, _rctx, 40, "cu32").add(reflection::v1::compacted{_rthis.cu64_}, _rctx, 41, "cu64");
        _rr.add(reflection::v1::compacted{_rthis.ci64_}, _rctx, 42, "ci64").add(reflection::v1::compacted{_rthis.ci32_}, _rctx, 43, "ci32");
        _rr.add(reflection::v1::compacted{_rthis.ci8_}, _rctx, 44, "ci8");
    }

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("b"sv, rthis.b_), _rc);
        _rr(id<2>, make("i8"sv, rthis.i8_), _rc);
        _rr(id<3>, make("u8"sv, rthis.u8_), _rc);
        _rr(id<4>, make("i16"sv, rthis.i16_), _rc);
        _rr(id<5>, make("u16"sv, rthis.u16_), _rc);
        _rr(id<6>, make("i32"sv, rthis.i32_), _rc);
        _rr(id<7>, make("u32"sv, rthis.u32_), _rc);
        _rr(id<8>, make("i64"sv, rthis.i64_), _rc);
        _rr(id<9>, make("u64"sv, rthis.u64_), _rc);
        _rr(id<10>, make("c"sv, rthis.c_), _rc);
        _rr(id<11>, make("s"sv, rthis.s_).with_max_size(10000), _rc);
        _rr(id<12>, make("vc"sv, rthis.vc_), _rc);
        _rr(id<13>, make("vu8"sv, rthis.vu8_), _rc);
        _rr(id<14>, make("vu32"sv, rthis.vu32_), _rc);
        _rr(id<15>, make("vsub"sv, rthis.vsub_).with_max_size(100), _rc);
        _rr(id<16>, make("dsub"sv, rthis.dsub_), _rc);
        _rr(id<17>, make("lsub"sv, rthis.lsub_), _rc);
        _rr(id<18>, make("msub"sv, rthis.msub_), _rc);
        _rr(id<19>, make("sset"sv, rthis.sset_), _rc);
        _rr(id<20>, make("mb"sv, rthis.mb_), _rc);
        _rr(id<21>, make("vb"sv, rthis.vb_), _rc);
        _rr(id<22>, make("db"sv, rthis.db_), _rc);
        _rr(id<23>, make("bs20"sv, rthis.bs20_), _rc);
        _rr(id<24>, make("bs100"sv, rthis.bs100_), _rc);
        _rr(id<25>, make("a8"sv, rthis.a8_), _rc);
        _rr(id<26>, make("abool"sv, rthis.abool_), _rc);
        _rr(id<27>, make("asub_cnt"sv, rthis.asub_cnt_), _rc);
        // the deserializer must see asub_cnt before limiting asub: defer
        _rr(id<28>, make("asub"sv, [&rthis](auto&& _rr, Ctx& _rc) -> void {
            _rr(id<1>, make("items"sv, rthis.asub_).with_max_size(rthis.asub_cnt_), _rc);
        }),
            _rc);
        _rr(id<29>, make("pair"sv, rthis.pair_), _rc);
        _rr(id<30>, make("tuple"sv, rthis.tuple_), _rc);
        _rr(id<31>, make("sub"sv, rthis.sub_), _rc);
        _rr(id<32>, make("sbase"sv, rthis.sbase_), _rc);
        _rr(id<33>, make("ubase"sv, rthis.ubase_), _rc);
        _rr(id<34>, make("null"sv, rthis.null_), _rc);
        _rr(id<35>, make("vbase"sv, rthis.vbase_), _rc);
        if constexpr (std::is_const_v<std::remove_reference_t<decltype(rthis)>>) {
            _rr(id<36>, make("stream"sv, rthis.iss_), _rc);
        } else {
            _rr(id<36>, make("stream"sv, rthis.oss_), _rc);
        }
        _rr(id<37>, make("tail"sv, rthis.tail_), _rc);
        _rr(id<38>, make("cu8"sv, rthis.cu8_).with_compacted(), _rc);
        _rr(id<39>, make("cu16"sv, rthis.cu16_).with_compacted(), _rc);
        _rr(id<40>, make("cu32"sv, rthis.cu32_).with_compacted(), _rc);
        _rr(id<41>, make("cu64"sv, rthis.cu64_).with_compacted(), _rc);
        _rr(id<42>, make("ci64"sv, rthis.ci64_).with_compacted(), _rc);
        _rr(id<43>, make("ci32"sv, rthis.ci32_).with_compacted(), _rc);
        _rr(id<44>, make("ci8"sv, rthis.ci8_).with_compacted(), _rc);
    }
};

template <class Init>
void register_types(Init& _rmap)
{
    if constexpr (requires { _rmap.template register_type<Message>(reflection::v2::TypeId{1}, reflection::v2::TypeName{"Message"}); }) {
        using reflection::v2::TypeId;
        using reflection::v2::TypeName;
        _rmap.template register_type<Message>(TypeId{1}, TypeName{"Message"});
        _rmap.template register_type<Derived1, Base>(TypeId{2}, TypeName{"Derived1"});
        _rmap.template register_type<Derived2, Base>(TypeId{300}, TypeName{"Derived2"});
    } else {
        _rmap.template registerType<Message>(0, 1, "Message");
        _rmap.template registerType<Derived1, Base>(0, 2, "Derived1");
        _rmap.template registerType<Derived2, Base>(0, 300, "Derived2");
    }
}

reflection::v1::TypeMap<SerializerV3T, DeserializerV3T> const& type_map_v3()
{
    static const reflection::v1::TypeMap<SerializerV3T, DeserializerV3T> type_map{[](auto& _rmap) { register_types(_rmap); }};
    return type_map;
}

reflection::v2::TypeMap<SerializerV4T, DeserializerV4T> const& type_map_v4()
{
    static const reflection::v2::TypeMap<SerializerV4T, DeserializerV4T> type_map{[](auto& _rmap) { register_types(_rmap); }};
    return type_map;
}

// The message travels as a polymorphic pointer, like an mprpc message.
template <class Ser>
string serialize(Ser& _rser, shared_ptr<Message> const& _msg, const size_t _chunk)
{
    Context      ctx;
    vector<char> buf(_chunk);
    string       out;
    auto         add = [&_msg](Ser& _rs, Context& _rctx) {
        if constexpr (std::is_same_v<Ser, SerializerV3T>) {
            _rs.add(_msg, _rctx, 0, "message");
        } else {
            _rs.add(_msg, _rctx);
        }
    };
    _msg->iss_.clear();
    _msg->iss_.seekg(0);
    // v3 packs bits into the bytes it finds in the buffer: hand it zeroes
    std::fill(buf.begin(), buf.end(), 0);
    ptrdiff_t rv = run_ser(_rser, buf, add, ctx);
    while (rv > 0) {
        out.append_range(span{buf}.first(static_cast<size_t>(rv)));
        std::fill(buf.begin(), buf.end(), 0);
        rv = run_ser(_rser, buf, ctx);
    }
    solid_check(rv == 0, "serialization failed: " << _rser.error().message());
    solid_check(_rser.empty());
    return out;
}

template <class Des>
shared_ptr<Message> deserialize(Des& _rdes, string const& _data, const size_t _chunk)
{
    Context             ctx;
    shared_ptr<Message> msg;
    size_t              offset = 0;
    size_t              sz     = std::min(_chunk, _data.size());
    auto                add    = [&msg](Des& _rd, Context& _rctx) {
        if constexpr (std::is_same_v<Des, DeserializerV3T>) {
            _rd.add(msg, _rctx, 0, "message");
        } else {
            _rd.add(msg, _rctx);
        }
    };
    ptrdiff_t rv = run_des(_rdes, _data, 0, sz, add, ctx);
    while (true) {
        solid_check(rv >= 0, "deserialization failed: " << _rdes.error().message());
        offset += static_cast<size_t>(rv);
        if (_rdes.empty()) {
            break;
        }
        solid_check(offset < _data.size(), "deserialization incomplete");
        sz = std::min(_chunk, _data.size() - offset);
        rv = run_des(_rdes, _data, offset, sz, ctx);
    }
    solid_check(offset == _data.size(), "unconsumed bytes " << _data.size() - offset);
    solid_check(msg);
    return msg;
}

void check_compatibility(const size_t _seed, const size_t _stream_size)
{
    auto msg = std::make_shared<Message>();
    msg->populate(_seed, _stream_size);

    size_t count = 0;
    for (size_t chunk = 16; chunk <= 70000; chunk += (chunk < 128 ? 1 : chunk / 3)) {
        SerializerV3T ser_v3{reflection::v1::metadata::factory, type_map_v3()};
        SerializerV4T ser_v4{type_map_v4()};

        const string data_v3 = serialize(ser_v3, msg, chunk);
        const string data_v4 = serialize(ser_v4, msg, chunk);

        if (data_v3 != data_v4) {
            size_t pos = 0;
            while (pos < data_v3.size() && pos < data_v4.size() && data_v3[pos] == data_v4[pos]) {
                ++pos;
            }
            solid_throw("v3/v4 data differ: chunk = " << chunk << " sizes = " << data_v3.size() << '/' << data_v4.size() << " first difference at " << pos);
        }

        // cross deserialization, on assorted input chunk sizes
        for (const size_t dchunk : {size_t{1}, size_t{2}, size_t{7}, chunk, data_v3.size()}) {
            DeserializerV4T des_v4{type_map_v4()};
            solid_check(*deserialize(des_v4, data_v3, dchunk) == *msg, "v4 from v3 differs: chunk " << chunk << '/' << dchunk);

            DeserializerV3T des_v3{reflection::v1::metadata::factory, type_map_v3()};
            solid_check(*deserialize(des_v3, data_v4, dchunk) == *msg, "v3 from v4 differs: chunk " << chunk << '/' << dchunk);
        }
        ++count;
    }
    cout << "seed " << _seed << " stream " << _stream_size << ": " << count << " buffer sizes checked" << endl;
}

// A lone 0x80 decodes as a UINT64_MAX size: both versions must reject it
// instead of trying to allocate it, even when the field has no limit.
template <class Des, class T>
ErrorConditionT deserialize_max_size(Des& _rdes, T& _rvalue)
{
    Context      ctx;
    const string data("\x80", 1);
    auto         add = [&_rvalue](Des& _rd, Context& _rctx) {
        if constexpr (std::is_same_v<Des, DeserializerV3T>) {
            _rd.add(_rvalue, _rctx, 0, "value");
        } else {
            _rd.add(_rvalue, _rctx);
        }
    };
    solid_check(run_des(_rdes, data, 0, data.size(), add, ctx) == -1);
    return _rdes.error();
}

template <class Des, class... Args>
void check_max_size(Args const&... _args)
{
    constexpr bool is_v3                 = std::is_same_v<Des, DeserializerV3T>;
    auto const&    error_limit_string    = is_v3 ? serialization::v3::error_limit_string : serialization::v4::error_limit_string;
    auto const&    error_limit_container = is_v3 ? serialization::v3::error_limit_container : serialization::v4::error_limit_container;
    {
        Des    des{_args...};
        string value;
        solid_check(deserialize_max_size(des, value) == error_limit_string);
    }
    {
        Des             des{_args...};
        vector<uint8_t> value;
        solid_check(deserialize_max_size(des, value) == error_limit_string);
    }
    {
        Des            des{_args...};
        vector<string> value;
        solid_check(deserialize_max_size(des, value) == error_limit_container);
    }
}

} // namespace

int test_compat_v3(int /*argc*/, char* /*argv*/[])
{
    check_max_size<DeserializerV4T>(type_map_v4());
    check_max_size<DeserializerV3T>(reflection::v1::metadata::factory, type_map_v3());
    check_compatibility(0, 0);
    check_compatibility(1, 100);
    check_compatibility(2, 100000);
    return 0;
}
