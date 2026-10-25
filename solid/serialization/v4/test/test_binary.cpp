#include <array>
#include <bitset>
#include <cstdint>
#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "solid/serialization/v4/serialization.hpp"
#include "solid/system/exception.hpp"

using namespace std;
using namespace solid;
using namespace std::literals;

namespace {

struct Context {
    size_t callable_count = 0;
};

enum struct ColorE : uint16_t {
    Red   = 1,
    Green = 0x1234,
    Blue  = 0xFFFE,
};

std::string_view to_string(const ColorE _c)
{
    switch (_c) {
    case ColorE::Red:
        return "red";
    case ColorE::Green:
        return "green";
    case ColorE::Blue:
        return "blue";
    }
    return "";
}

std::optional<ColorE> from_string(std::type_identity<ColorE>, std::string_view _sv)
{
    if (_sv == "red") {
        return ColorE::Red;
    }
    if (_sv == "green") {
        return ColorE::Green;
    }
    if (_sv == "blue") {
        return ColorE::Blue;
    }
    return {};
}

struct Sub {
    uint64_t a_ = 0;
    string   b_;

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("a"sv, rthis.a_), _rc);
        _rr(id<2>, make("b"sv, rthis.b_), _rc);
    }

    bool operator==(Sub const&) const  = default;
    auto operator<=>(Sub const&) const = default;
};

struct Empty {
    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& /*rthis*/, Reflector&& /*_rr*/, Ctx& /*_rc*/)
    {
    }
    bool operator==(Empty const&) const = default;
};

using VariantT = std::variant<std::monostate, uint32_t, std::string, Sub>;

struct Full {
    bool                            b_   = false;
    int8_t                          i8_  = 0;
    uint8_t                         u8_  = 0;
    int16_t                         i16_ = 0;
    uint32_t                        u32_ = 0;
    int64_t                         i64_ = 0;
    uint64_t                        u64_ = 0;
    char                            c_   = 0;
    ColorE                          color_{ColorE::Red};
    string                          s_;
    string                          empty_s_;
    vector<uint32_t>                vu32_;
    vector<char>                    vc_;
    vector<int16_t>                 vi16_;
    vector<Sub>                     vsub_;
    deque<Sub>                      dsub_;
    list<string>                    lst_;
    map<string, Sub>                msub_;
    set<string>                     sset_;
    multiset<int32_t>               mset_;
    unordered_map<uint32_t, string> umap_;
    vector<bool>                    vb_;
    deque<bool>                     db_;
    bitset<20>                      bs20_;
    bitset<1000>                    bs1000_;
    array<uint8_t, 10>              a8_{};
    array<bool, 5>                  abool_{};
    array<uint32_t, 3>              a32_{};
    array<Sub, 6>                   asub_{};
    uint16_t                        asub_cnt_ = 0;
    pair<int32_t, string>           pair_;
    tuple<uint8_t, string, Sub>     tuple_;
    optional<Sub>                   osub_;
    optional<string>                onull_;
    optional<uint16_t>              ou16_;
    VariantT                        var_;
    vector<VariantT>                vvar_;
    unique_ptr<Sub>                 usub_;
    shared_ptr<Sub>                 ssub_;
    unique_ptr<Sub>                 unull_;
    vector<vector<uint8_t>>         vvu8_;
    map<uint16_t, vector<Sub>>      mvsub_;
    vector<Empty>                   vempty_;
    Empty                           empty_;
    bool                            cond_ = false;
    vector<Sub>                     cond_true_;
    string                          cond_false_;

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("b"sv, rthis.b_), _rc);
        _rr(id<2>, make("i8"sv, rthis.i8_), _rc);
        _rr(id<3>, make("u8"sv, rthis.u8_), _rc);
        _rr(id<4>, make("i16"sv, rthis.i16_), _rc);
        _rr(id<5>, make("u32"sv, rthis.u32_), _rc);
        _rr(id<6>, make("i64"sv, rthis.i64_), _rc);
        _rr(id<7>, make("u64"sv, rthis.u64_), _rc);
        _rr(id<8>, make("c"sv, rthis.c_), _rc);
        _rr(id<9>, make("color"sv, rthis.color_), _rc);
        _rr(id<10>, make("s"sv, rthis.s_).with_max_size(4096), _rc);
        _rr(id<11>, make("empty_s"sv, rthis.empty_s_), _rc);
        _rr(id<12>, make("vu32"sv, rthis.vu32_), _rc);
        _rr(id<13>, make("vc"sv, rthis.vc_), _rc);
        _rr(id<14>, make("vi16"sv, rthis.vi16_), _rc);
        _rr(id<15>, make("vsub"sv, rthis.vsub_).with_max_size(100), _rc);
        _rr(id<16>, make("dsub"sv, rthis.dsub_), _rc);
        _rr(id<17>, make("lst"sv, rthis.lst_), _rc);
        _rr(id<18>, make("msub"sv, rthis.msub_), _rc);
        _rr(id<19>, make("sset"sv, rthis.sset_), _rc);
        _rr(id<20>, make("mset"sv, rthis.mset_), _rc);
        _rr(id<21>, make("umap"sv, rthis.umap_), _rc);
        _rr(id<22>, make("vb"sv, rthis.vb_), _rc);
        _rr(id<23>, make("db"sv, rthis.db_), _rc);
        _rr(id<24>, make("bs20"sv, rthis.bs20_), _rc);
        _rr(id<25>, make("bs1000"sv, rthis.bs1000_), _rc);
        _rr(id<26>, make("a8"sv, rthis.a8_), _rc);
        _rr(id<27>, make("abool"sv, rthis.abool_), _rc);
        _rr(id<28>, make("a32"sv, rthis.a32_), _rc);
        _rr(id<29>, make("asub_cnt"sv, rthis.asub_cnt_), _rc);
        // the count is known only once asub_cnt is in: defer through a callable
        _rr(id<30>, make("asub"sv, [&rthis](auto&& _rr, Ctx& _rc) -> void {
            _rr(id<1>, make("items"sv, rthis.asub_).with_max_size(rthis.asub_cnt_), _rc);
        }),
            _rc);
        _rr(id<31>, make("pair"sv, rthis.pair_), _rc);
        _rr(id<32>, make("tuple"sv, rthis.tuple_), _rc);
        _rr(id<33>, make("osub"sv, rthis.osub_), _rc);
        _rr(id<34>, make("onull"sv, rthis.onull_), _rc);
        _rr(id<35>, make("ou16"sv, rthis.ou16_), _rc);
        _rr(id<36>, make("var"sv, rthis.var_), _rc);
        _rr(id<37>, make("vvar"sv, rthis.vvar_), _rc);
        _rr(id<38>, make("usub"sv, rthis.usub_), _rc);
        _rr(id<39>, make("ssub"sv, rthis.ssub_), _rc);
        _rr(id<40>, make("unull"sv, rthis.unull_), _rc);
        _rr(id<41>, make("vvu8"sv, rthis.vvu8_), _rc);
        _rr(id<42>, make("mvsub"sv, rthis.mvsub_), _rc);
        _rr(id<43>, make("vempty"sv, rthis.vempty_), _rc);
        _rr(id<44>, make("empty"sv, rthis.empty_), _rc);
        _rr(id<45>, make("cond"sv, rthis.cond_), _rc);
        _rr(id<46>, make("cond_fields"sv, [&rthis](auto&& _rr, Ctx& _rc) -> void {
            ++_rc.callable_count;
            if (rthis.cond_) {
                _rr(id<1>, make("cond_true"sv, rthis.cond_true_), _rc);
            } else {
                _rr(id<2>, make("cond_false"sv, rthis.cond_false_), _rc);
            }
        }),
            _rc);
    }

    bool operator==(Full const& _o) const
    {
        auto ptr_eq = [](auto const& _p1, auto const& _p2) {
            return (!_p1 && !_p2) || (_p1 && _p2 && *_p1 == *_p2);
        };
        bool asub_eq = asub_cnt_ == _o.asub_cnt_;
        for (size_t i = 0; asub_eq && i < asub_cnt_; ++i) {
            asub_eq = asub_[i] == _o.asub_[i];
        }
        return b_ == _o.b_ && i8_ == _o.i8_ && u8_ == _o.u8_ && i16_ == _o.i16_ && u32_ == _o.u32_ && i64_ == _o.i64_ && u64_ == _o.u64_ && c_ == _o.c_
            && color_ == _o.color_ && s_ == _o.s_ && empty_s_ == _o.empty_s_ && vu32_ == _o.vu32_ && vc_ == _o.vc_ && vi16_ == _o.vi16_
            && vsub_ == _o.vsub_ && dsub_ == _o.dsub_ && lst_ == _o.lst_ && msub_ == _o.msub_ && sset_ == _o.sset_ && mset_ == _o.mset_
            && umap_ == _o.umap_ && vb_ == _o.vb_ && db_ == _o.db_ && bs20_ == _o.bs20_ && bs1000_ == _o.bs1000_ && a8_ == _o.a8_
            && abool_ == _o.abool_ && a32_ == _o.a32_ && asub_eq && pair_ == _o.pair_ && tuple_ == _o.tuple_ && osub_ == _o.osub_
            && onull_ == _o.onull_ && ou16_ == _o.ou16_ && var_ == _o.var_ && vvar_ == _o.vvar_ && ptr_eq(usub_, _o.usub_)
            && ptr_eq(ssub_, _o.ssub_) && ptr_eq(unull_, _o.unull_) && vvu8_ == _o.vvu8_ && mvsub_ == _o.mvsub_
            && vempty_ == _o.vempty_ && cond_ == _o.cond_ && cond_true_ == _o.cond_true_ && cond_false_ == _o.cond_false_;
    }

    void populate(const bool _cond)
    {
        b_     = true;
        i8_    = -100;
        u8_    = 200;
        i16_   = -12345;
        u32_   = 0xDEADBEEF;
        i64_   = -1234567890123456789LL;
        u64_   = 0xFEDCBA9876543210ULL;
        c_     = 'x';
        color_ = ColorE::Green;
        for (size_t i = 0; i < 300; ++i) {
            s_ += static_cast<char>('a' + (i % 26));
        }
        for (uint32_t i = 0; i < 50; ++i) {
            vu32_.push_back(i * 0x01010101u);
            vc_.push_back(static_cast<char>('A' + (i % 26)));
            vi16_.push_back(static_cast<int16_t>(-static_cast<int>(i) * 100));
        }
        for (uint64_t i = 0; i < 20; ++i) {
            Sub sub{.a_ = i * 1000003, .b_ = "sub " + std::to_string(i)};
            vsub_.push_back(sub);
            dsub_.push_front(sub);
            lst_.push_back(sub.b_ + "!");
            msub_["key" + std::to_string(i)] = sub;
            sset_.insert(sub.b_);
            mset_.insert(static_cast<int32_t>(i % 5));
            umap_[static_cast<uint32_t>(i * 7)] = sub.b_;
        }
        for (size_t i = 0; i < 77; ++i) {
            vb_.push_back((i % 3) == 0);
            db_.push_back((i % 2) == 0);
        }
        for (size_t i = 0; i < bs20_.size(); ++i) {
            bs20_[i] = (i % 3) == 1;
        }
        for (size_t i = 0; i < bs1000_.size(); ++i) {
            bs1000_[i] = (i % 7) == 1;
        }
        for (size_t i = 0; i < a8_.size(); ++i) {
            a8_[i] = static_cast<uint8_t>(i * 25);
        }
        abool_ = {true, false, true, true, false};
        a32_   = {1, 0x10000, 0xFFFFFFFF};
        for (size_t i = 0; i < asub_.size(); ++i) {
            asub_[i] = Sub{.a_ = i, .b_ = "asub " + std::to_string(i)};
        }
        asub_cnt_ = 4;
        pair_     = {-7, "pair"};
        tuple_    = {42, "tuple", Sub{.a_ = 3, .b_ = "tuple sub"}};
        osub_     = Sub{.a_ = 11, .b_ = "optional"};
        ou16_     = 65000;
        var_      = Sub{.a_ = 12, .b_ = "variant"};
        vvar_     = {std::monostate{}, 123u, "variant str"s, Sub{.a_ = 13, .b_ = "in vector"}};
        usub_     = std::make_unique<Sub>(Sub{.a_ = 14, .b_ = "unique"});
        ssub_     = std::make_shared<Sub>(Sub{.a_ = 15, .b_ = "shared"});
        vvu8_     = {{}, {1, 2, 3}, std::vector<uint8_t>(200, 0x5A)};
        mvsub_[7] = {Sub{.a_ = 16, .b_ = "m1"}, Sub{.a_ = 17, .b_ = "m2"}};
        mvsub_[9] = {};
        vempty_.resize(3);
        cond_ = _cond;
        if (_cond) {
            cond_true_ = {Sub{.a_ = 18, .b_ = "cond 1"}, Sub{.a_ = 19, .b_ = "cond 2"}};
        } else {
            cond_false_ = "condition is false";
        }
    }
};

struct Streams {
    uint32_t                   before_ = 0;
    mutable std::istringstream iss_;
    std::ostringstream         oss_;
    uint32_t                   after_ = 0;

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("before"sv, rthis.before_), _rc);
        if constexpr (std::is_const_v<std::remove_reference_t<decltype(rthis)>>) {
            _rr(id<2>, make("stream"sv, rthis.iss_), _rc);
        } else {
            _rr(id<2>, make("stream"sv, rthis.oss_), _rc);
        }
        _rr(id<3>, make("after"sv, rthis.after_), _rc);
    }
};

// Streams limited through the descriptors: at most size_ bytes are sent,
// more than max_size_ received is an error.
struct LimitedStreams {
    uint64_t                   size_     = ~0ULL;
    uint64_t                   max_size_ = ~0ULL;
    mutable std::istringstream iss_;
    std::ostringstream         oss_;
    uint64_t                   progress_total_ = 0;
    bool                       progress_done_  = false;

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        if constexpr (std::is_const_v<std::remove_reference_t<decltype(rthis)>>) {
            _rr(id<1>, make("stream"sv, rthis.iss_).with_size(rthis.size_), _rc);
        } else {
            auto progress = [&rthis](std::ostream&, uint64_t _total, bool _done, Ctx&) {
                rthis.progress_total_ = _total;
                rthis.progress_done_  = _done;
            };
            // with_max_size after with_progress must keep the progress callback
            _rr(id<1>, make("stream"sv, rthis.oss_).with_progress(progress).with_max_size(rthis.max_size_), _rc);
        }
    }
};

struct Limited {
    string      s_;
    vector<Sub> v_;

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("s"sv, rthis.s_).with_max_size(8), _rc);
        _rr(id<2>, make("v"sv, rthis.v_).with_max_size(2), _rc);
    }
};

#ifdef SOLID_USE_REFLECTION
// Natively reflected (no solidReflectV2): the skipped members are not
// reflected - not even instantiated, so their types need not be reflectable.
struct NativeSkip {
    uint32_t                                               before_ = 0;
    [[= solid::reflection::v2::annotate::skip]] std::mutex mtx_;
    [[= solid::reflection::v2::annotate::skip]] uint64_t   cache_ = 0;
    [[= solid::reflection::v2::annotate::skip]] Sub*       praw_  = nullptr;
    string                                                 after_;
};
#endif

// Opt-in compacted integers: stored like sizes instead of on sizeof(T) bytes.
struct Compacted {
    uint8_t  u8_  = 0;
    uint16_t u16_ = 0;
    uint64_t u64_ = 0;
    uint64_t max_ = 0;
    int32_t  i32_ = 0;
    uint32_t raw_ = 0; // not compacted

    bool operator==(Compacted const&) const = default;

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("u8"sv, rthis.u8_).with_compacted(), _rc);
        _rr(id<2>, make("u16"sv, rthis.u16_).with_max(1000).with_compacted(), _rc);
        _rr(id<3>, make("u64"sv, rthis.u64_).with_compacted(), _rc);
        _rr(id<4>, make("max"sv, rthis.max_).with_compacted(), _rc);
        _rr(id<5>, make("i32"sv, rthis.i32_).with_compacted(), _rc);
        _rr(id<6>, make("raw"sv, rthis.raw_), _rc);
    }
};

// The same compacted fields with other widths: all bits set crosses widths.
template <class U, class S>
struct CompactedWidth {
    U u_ = 0;
    S s_ = 0;

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("u"sv, rthis.u_).with_compacted(), _rc);
        _rr(id<2>, make("s"sv, rthis.s_).with_compacted(), _rc);
    }
};

// Reads back what Compacted::u64_ was written with into an uint8_t.
struct CompactedNarrow {
    uint8_t  u8_  = 0;
    uint16_t u16_ = 0;
    uint8_t  u64_ = 0;

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("u8"sv, rthis.u8_).with_compacted(), _rc);
        _rr(id<2>, make("u16"sv, rthis.u16_).with_compacted(), _rc);
        _rr(id<3>, make("u64"sv, rthis.u64_).with_compacted(), _rc);
    }
};

#ifdef SOLID_USE_REFLECTION
struct NativeCompacted {
    [[= solid::reflection::v2::annotate::compacted]] uint64_t                                                 id_    = 0;
    [[ = solid::reflection::v2::annotate::compacted, = solid::reflection::v2::annotate::max(100u) ]] uint32_t count_ = 0;
    uint16_t                                                                                                  raw_   = 0;
};
#endif

struct Unlimited {
    string      s_;
    vector<Sub> v_;

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("s"sv, rthis.s_), _rc);
        _rr(id<2>, make("v"sv, rthis.v_), _rc);
    }
};

using SerializerT   = serialization::v4::binary::Serializer<Context>;
using DeserializerT = serialization::v4::binary::Deserializer<Context>;

template <class T>
string serialize(T const& _rt, const size_t _chunk, Context& _rctx)
{
    SerializerT  ser;
    vector<char> buf(_chunk);
    string       out;

    ptrdiff_t rv = ser.run(as_writable_bytes(span{buf}), [&_rt](SerializerT& _rs, Context& _rctx) { _rs.add(_rt, _rctx); }, _rctx);
    while (rv > 0) {
        out.append_range(span{buf}.first(static_cast<size_t>(rv)));
        rv = ser.run(as_writable_bytes(span{buf}), _rctx);
    }
    solid_check(rv == 0, "serialization failed: " << ser.error().message());
    solid_check(ser.empty());
    return out;
}

// Feeds _data _chunk bytes at a time; returns the deserializer error.
template <class T>
ErrorConditionT deserialize(string const& _data, T& _rt, const size_t _chunk, Context& _rctx)
{
    DeserializerT des;
    size_t        offset = 0;
    size_t        sz     = std::min(_chunk, _data.size());

    ptrdiff_t rv = des.run(as_bytes(span{_data}).first(sz), [&_rt](DeserializerT& _rd, Context& _rctx) { _rd.add(_rt, _rctx); }, _rctx);
    while (rv >= 0) {
        offset += static_cast<size_t>(rv);
        if (des.empty()) {
            solid_check(offset == _data.size(), "unconsumed bytes: " << _data.size() - offset);
            return {};
        }
        solid_check(static_cast<size_t>(rv) == sz, "partial consumption with pending work");
        solid_check(offset < _data.size(), "deserialization incomplete");
        sz = std::min(_chunk, _data.size() - offset);
        rv = des.run(as_bytes(span{_data}).subspan(offset, sz), _rctx);
    }
    return des.error();
}

void test_full()
{
    for (const bool cond : {true, false}) {
        Full full;
        full.populate(cond);

        Context      ctx;
        const string reference = serialize(full, 1024 * 1024, ctx);
        solid_check(ctx.callable_count == 1);
        cout << "full(" << cond << ") serialized size = " << reference.size() << endl;

        for (size_t chunk = 1; chunk <= 300; chunk += (chunk < 40 ? 1 : 37)) {
            const string data = serialize(full, chunk, ctx);
            solid_check(data == reference, "serialized data differs for chunk size " << chunk);
        }

        for (size_t chunk = 1; chunk <= reference.size() + 1; chunk += (chunk < 64 ? 1 : 131)) {
            Full       copy;
            Context    dctx;
            const auto err = deserialize(reference, copy, chunk, dctx);
            solid_check(!err, "deserialization failed for chunk size " << chunk << ": " << err.message());
            solid_check(copy == full, "deserialized object differs for chunk size " << chunk);
            solid_check(dctx.callable_count == 1);
        }
        // deserializing into an object already holding data must yield the same
        {
            Full copy;
            copy.populate(cond);
            copy.s_ = "previous";
            copy.vsub_.resize(3);
            copy.msub_["extra"] = Sub{};
            copy.vb_.push_back(true);
            copy.bs1000_.flip();
            copy.var_ = 5u;
            copy.osub_.reset();
            copy.unull_ = std::make_unique<Sub>();
            Context dctx;
            solid_check(!deserialize(reference, copy, 7, dctx));
            solid_check(copy == full);
        }
    }
}

void test_streams()
{
    string text;
    for (size_t i = 0; i < 100000; ++i) {
        text += static_cast<char>('0' + (i % 10));
    }

    for (const size_t chunk : {3UL, 4UL, 5UL, 17UL, 100UL, 4096UL, 70000UL}) {
        Streams in;
        in.before_ = 0x11223344;
        in.after_  = 0x55667788;
        in.iss_.str(text);

        Context      ctx;
        const string data = serialize(in, chunk, ctx);

        for (const size_t dchunk : {1UL, 2UL, 3UL, 1000UL, 1000000UL}) {
            Streams out;
            solid_check(!deserialize(data, out, dchunk, ctx));
            solid_check(out.before_ == in.before_ && out.after_ == in.after_);
            solid_check(out.oss_.str() == text, "stream differs " << chunk << ' ' << dchunk);
        }
    }
}

void test_limited_streams()
{
    Context      ctx;
    const string text(50000, 'z');
    {
        LimitedStreams in;
        in.size_ = 12345;
        in.iss_.str(text);
        const string   data = serialize(in, 1000, ctx);
        LimitedStreams out;
        solid_check(!deserialize(data, out, 333, ctx));
        solid_check(out.oss_.str() == text.substr(0, 12345));
        solid_check(out.progress_total_ == 12345 && out.progress_done_);
    }
    {
        LimitedStreams in;
        in.iss_.str(text);
        const string   data = serialize(in, 1000, ctx);
        LimitedStreams out;
        out.max_size_ = 20000;
        solid_check(deserialize(data, out, 333, ctx) == serialization::v4::error_limit_stream);
        solid_check(out.oss_.str().size() <= 20000);
    }
}

void test_native_skip()
{
#ifdef SOLID_USE_REFLECTION
    Context    ctx;
    NativeSkip in;
    in.before_ = 0x01020304;
    in.cache_  = 7;
    in.after_  = "after";

    // only before_ and after_ are on the wire
    const string data = serialize(in, 3, ctx);
    solid_check(data == string("\x04\x03\x02\x01\x05"
                               "after",
                    10));

    NativeSkip out;
    out.cache_ = 99;
    solid_check(!deserialize(data, out, 1, ctx));
    solid_check(out.before_ == in.before_ && out.after_ == in.after_ && out.cache_ == 99 && out.praw_ == nullptr);

    // the remaining members keep their declaration ids
    vector<pair<size_t, string>> fields;
    solid::reflection::v2::reflect(in, [&fields](auto _id, auto&& _rfield, Context&) { fields.emplace_back(static_cast<size_t>(_id.value), string(_rfield.name)); }, ctx);
    solid_check((fields == vector<pair<size_t, string>>{{1, "before_"}, {5, "after_"}}));
#endif
}

void test_compacted()
{
    using solid::reflection::v2::make;
    static_assert(!decltype(make(""sv, std::declval<uint64_t&>()))::compacted);
    static_assert(decltype(make(""sv, std::declval<uint64_t&>()).with_compacted())::compacted);
    {
        uint16_t v = 0;
        auto     f = make("v"sv, v).with_min(2).with_compacted();
        solid_check(f.min == 2 && f.max == std::numeric_limits<uint16_t>::max());
    }

    Context   ctx;
    Compacted in;
    in.u8_  = 5;
    in.u16_ = 300;
    in.u64_ = 0x0102030405ULL;
    in.max_ = std::numeric_limits<uint64_t>::max();
    in.i32_ = -1;
    in.raw_ = 7;

    const string expected("\x05"
                          "\xfe\x2c\x01"
                          "\xfb\x05\x04\x03\x02\x01"
                          "\x80" // UINT64_MAX marker
                          "\x80" // -1, sign extended to UINT64_MAX
                          "\x07\x00\x00\x00",
        1 + 3 + 6 + 1 + 1 + 4);
    for (const size_t chunk : {size_t{1}, size_t{2}, size_t{3}, size_t{5}, size_t{64}}) {
        solid_check(serialize(in, chunk, ctx) == expected, "chunk " << chunk);
        Compacted out;
        solid_check(!deserialize(expected, out, chunk, ctx), "chunk " << chunk);
        solid_check(out == in, "chunk " << chunk);
    }

    // a value that does not fit the field
    for (const size_t chunk : {size_t{1}, size_t{64}}) {
        CompactedNarrow out;
        solid_check(deserialize(expected, out, chunk, ctx) == serialization::v4::error_compacted_integer);
    }
    // all bits set is read as all bits set in the reader's type
    {
        CompactedWidth<uint32_t, int16_t> win;
        win.u_            = std::numeric_limits<uint32_t>::max();
        win.s_            = -1;
        const string data = serialize(win, 1, ctx);
        solid_check(data == string("\x80\x80", 2));

        CompactedWidth<uint64_t, int64_t> wide;
        solid_check(!deserialize(data, wide, 1, ctx));
        solid_check(wide.u_ == std::numeric_limits<uint64_t>::max() && wide.s_ == -1);

        CompactedWidth<uint8_t, int8_t> narrow;
        solid_check(!deserialize(data, narrow, 1, ctx));
        solid_check(narrow.u_ == 0xFF && narrow.s_ == -1);

        // anything else keeps its value, so it may not fit
        win.u_ = std::numeric_limits<uint32_t>::max() - 1;
        solid_check(deserialize(serialize(win, 1, ctx), narrow, 1, ctx) == serialization::v4::error_compacted_integer);
        // a sign extended value fits any signed width it is in range of
        win.u_ = 1;
        win.s_ = -2;
        solid_check(!deserialize(serialize(win, 1, ctx), narrow, 1, ctx) && narrow.u_ == 1 && narrow.s_ == -2);
    }
    // a multi-byte negative value
    {
        Compacted neg     = in;
        neg.i32_          = -300;
        const string data = serialize(neg, 1, ctx);
        solid_check(data.size() == expected.size() + 8 && data.substr(11, 9) == string("\xf8\xd4\xfe\xff\xff\xff\xff\xff\xff", 9));
        Compacted out;
        solid_check(!deserialize(data, out, 1, ctx) && out == neg);
    }

#ifdef SOLID_USE_REFLECTION
    NativeCompacted nin;
    nin.id_            = 1000;
    nin.count_         = 3;
    nin.raw_           = 0x0102;
    const string ndata = serialize(nin, 2, ctx);
    solid_check(ndata == string("\xfe\xe8\x03\x03\x02\x01", 6));
    NativeCompacted nout;
    solid_check(!deserialize(ndata, nout, 1, ctx));
    solid_check(nout.id_ == nin.id_ && nout.count_ == nin.count_ && nout.raw_ == nin.raw_);

    // annotations other than compacted still reach the compacted descriptor
    auto check_max = [](auto, auto&& _rfield, Context&) {
        using FieldT = std::remove_cvref_t<decltype(_rfield)>;
        if constexpr (FieldT::compacted && std::is_same_v<typename FieldT::value_type, uint32_t>) {
            solid_check(_rfield.max == 100);
        }
    };
    solid::reflection::v2::reflect(nin, check_max, ctx);
#endif
}

void test_errors()
{
    Context ctx;
    // limits on the serializer side
    {
        Limited lim;
        lim.s_ = "too long for the limit";
        SerializerT ser;
        char        buf[256];
        solid_check(ser.run(as_writable_bytes(span{buf}), [&](SerializerT& _rs, Context& _rctx) { _rs.add(lim, _rctx); }, ctx) == -1);
        solid_check(ser.error() == serialization::v4::error_limit_string);
    }
    // limits on the deserializer side
    {
        Unlimited unl;
        unl.s_            = "too long for the limit";
        const string data = serialize(unl, 64, ctx);
        Limited      lim;
        solid_check(deserialize(data, lim, 3, ctx) == serialization::v4::error_limit_string);
    }
    {
        Unlimited unl;
        unl.v_.resize(3);
        const string data = serialize(unl, 64, ctx);
        Limited      lim;
        solid_check(deserialize(data, lim, 1, ctx) == serialization::v4::error_limit_container);
    }
    // corrupt compacted size
    {
        const string data("\x81", 1); // -127: more than 8 value bytes
        string       s;
        solid_check(deserialize(data, s, 1, ctx) == serialization::v4::error_compacted_integer);
        solid_check(deserialize(data, s, 10, ctx) == serialization::v4::error_compacted_integer);
    }
    // bad variant index
    {
        const string data("\x09", 1);
        VariantT     var;
        solid_check(deserialize(data, var, 1, ctx) == serialization::v4::error_variant_index);
    }
    // a failed serializer is reusable after clear()
    {
        Limited lim;
        lim.s_ = "too long for the limit";
        SerializerT                ser;
        std::array<std::byte, 256> buf;
        solid_check(ser.run(buf, [&](SerializerT& _rs, Context& _rctx) { _rs.add(lim, _rctx); }, ctx) == -1);
        ser.clear();
        lim.s_ = "ok";
        solid_check(ser.run(buf, [&](SerializerT& _rs, Context& _rctx) { _rs.add(lim, _rctx); }, ctx) > 0);
    }
}

void test_scalars()
{
    Context ctx;
    // top level values other than reflectable objects
    const uint64_t v    = 0x0102030405060708ULL;
    const string   data = serialize(v, 3, ctx);
    solid_check(data == string("\x08\x07\x06\x05\x04\x03\x02\x01", 8));
    uint64_t w = 0;
    solid_check(!deserialize(data, w, 1, ctx));
    solid_check(w == v);

    const vector<string> vs{"a", "bb", string(200, 'c')};
    vector<string>       ws;
    solid_check(!deserialize(serialize(vs, 5, ctx), ws, 2, ctx));
    solid_check(vs == ws);

    // compacted encodings
    for (const uint64_t value : {0ULL, 127ULL, 128ULL, 255ULL, 256ULL, 0xFFFFULL, 0x10000ULL, 0xFFFFFFFFFFULL, ~0ULL}) {
        char       buf[serialization::v4::binary::max_compacted_size];
        const auto sz = serialization::v4::binary::store_compact(buf, value) - buf;
        solid_check(static_cast<size_t>(sz) == serialization::v4::binary::compact_size(value));
        solid_check(static_cast<size_t>(sz) == serialization::v4::binary::compact_size(buf, sizeof(buf)));
        uint64_t loaded = 0;
        solid_check(serialization::v4::binary::load_compact(buf, static_cast<size_t>(sz), loaded) == buf + sz && loaded == value);
        solid_check(serialization::v4::binary::load_compact(buf, static_cast<size_t>(sz) - 1, loaded) == nullptr);
    }
    {
        char buf[serialization::v4::binary::max_compacted_size];
        solid_check(serialization::v4::binary::store_compact(buf, ~0ULL) == buf + 1 && buf[0] == '\x80');
        // the marker loads as all bits set in the reader's type
        uint32_t narrow = 0;
        solid_check(serialization::v4::binary::load_compact(buf, 1, narrow) == buf + 1 && narrow == std::numeric_limits<uint32_t>::max());
        solid_check(serialization::v4::binary::store_compact(buf, sizeof(buf), uint16_t{0xFFFF}) == buf + 1 && buf[0] == '\x80');
    }
}

} // namespace

int test_binary(int /*argc*/, char* /*argv*/[])
{
    test_scalars();
    test_full();
    test_streams();
    test_limited_streams();
    test_native_skip();
    test_compacted();
    test_errors();
    cout << "done" << endl;
    return 0;
}
