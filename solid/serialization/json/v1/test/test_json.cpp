#include "solid/serialization/json/v1/deserialization.hpp"
#include "solid/serialization/json/v1/serialization.hpp"
#include "solid/system/exception.hpp"

#include <algorithm>
#include <array>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <deque>
#include <initializer_list>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

using namespace std;
using namespace solid::serialization::json;

namespace alpha {

struct Sub {
    uint64_t a_;
    string   b_;

    template <typename T, typename Reflector, typename Ctx>
    void solidReflectV2(this T& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(make<1>("a"sv, rthis.a_), _rc);
        _rr(make<2>("b"sv, rthis.b_), _rc);
    }
};

struct SubWrap {
    enum class FieldE {
        sub_lst,
        sub_ptr
    };
    std::list<Sub>       sub_lst_;
    std::shared_ptr<Sub> sub_ptr_;

    template <typename T, typename Reflector, typename Ctx>
    void solidReflectV2(this T& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(make<FieldE::sub_lst>("sub_lst"sv, rthis.sub_lst_), _rc);
        _rr(make<FieldE::sub_ptr>("sub_ptr"sv, rthis.sub_ptr_), _rc);
    }
};

static_assert(solid::reflection::v2::ReflectableC<Sub>);

enum class NumbersE {
    Zero = 0,
    One,
    Two,
    Three,
    Four,
    Five,
    Six,
    Seven,
    Eight,
    Nine,
};

std::string_view to_string(NumbersE const _value)
{
    switch (_value) {
    case NumbersE::Zero:
        return "Zero"sv;
    case NumbersE::One:
        return "One"sv;
    case NumbersE::Two:
        return "Two"sv;
    case NumbersE::Three:
        return "Three"sv;
    case NumbersE::Four:
        return "Four"sv;
    case NumbersE::Five:
        return "Five"sv;
    case NumbersE::Six:
        return "Six"sv;
    case NumbersE::Seven:
        return "Seven"sv;
    case NumbersE::Eight:
        return "Eight"sv;
    case NumbersE::Nine:
        return "Nine"sv;
    }
    return {};
}

std::optional<NumbersE> from_string(std::type_identity<NumbersE>, std::string_view _value_as_string)
{
    static const std::unordered_map<std::string_view, NumbersE> mapping{
        {"Zero"sv, NumbersE::Zero},
        {"One"sv, NumbersE::One},
        {"Two"sv, NumbersE::Two},
        {"Three"sv, NumbersE::Three},
        {"Four"sv, NumbersE::Four},
        {"Five"sv, NumbersE::Five},
        {"Six"sv, NumbersE::Six},
        {"Seven"sv, NumbersE::Seven},
        {"Eight"sv, NumbersE::Eight},
        {"Nine"sv, NumbersE::Nine},
    };
    auto const it = mapping.find(_value_as_string);
    if (it == mapping.end()) {
        return std::nullopt;
    }
    return it->second;
}

class One {
    bool                       vb_;
    uint32_t                   vu32_;
    string                     vs_;
    Sub                        sub_;
    std::vector<Sub>           sub_dq_;
    std::map<string, Sub>      sub_map_;
    vector<bool>               bool_vec_{true, false, true};
    std::array<Sub, 3>         sub_arr_;
    mutable std::stringstream  ioss_;
    mutable std::ostringstream oss_;
    mutable std::istringstream iss_;

public:
    using VariantT = std::variant<std::monostate, std::optional<Sub>, std::string, std::vector<Sub>>;
    std::tuple<std::string, uint64_t, Sub>    tuple_;
    std::unique_ptr<SubWrap>                  subw_ptr_;
    std::bitset<24>                           bits_;
    std::optional<SubWrap>                    subw_opt_;
    std::optional<std::string>                null_str_opt_;
    std::optional<std::string>                str_opt_;
    std::optional<std::vector<std::uint8_t>>  uint8_vec_opt_;
    std::optional<bool>                       bool_opt_;
    std::shared_ptr<bool>                     bool_ptr_;
    std::vector<VariantT>                     variant_vec_;
    NumbersE                                  number_{NumbersE::Zero};
    std::vector<std::pair<uint8_t, NumbersE>> numbers_{{0, NumbersE::Zero}, {1, NumbersE::One}, {2, NumbersE::Two}, {3, NumbersE::Three}, {4, NumbersE::Four}};

    static_assert(solid::reflection::v2::detail::is_unique_ptr_v<std::unique_ptr<SubWrap>>);

    One() = default;

    One(
        bool _b, uint32_t _u32, auto&& _s,
        Sub&&                                         _sub,
        std::initializer_list<Sub>                    _subs,
        std::initializer_list<std::pair<string, Sub>> _subs_map,
        std::array<Sub, 3> const&                     _sub_arr)
        : vb_{_b}
        , vu32_{_u32}
        , vs_{std::forward<decltype(_s)>(_s)}
        , sub_{std::move(_sub)}
        , sub_dq_{_subs}
        , sub_map_{_subs_map.begin(), _subs_map.end()}
        , sub_arr_{_sub_arr}
    {
    }

    template <typename T, typename Reflector, typename Ctx>
    void solidReflectV2(this T& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(make<1>("b"sv, rthis.vb_), _rc);
        _rr(make<2>("u32"sv, rthis.vu32_).set_max(1000).set_min(10), _rc);
        _rr(make<3>("s"sv, rthis.vs_).set_max_size(1000), _rc);
        _rr(make<4>("sub"sv, rthis.sub_), _rc);
        _rr(make<5>("sub_dq"sv, rthis.sub_dq_), _rc);
        _rr(make<6>("sub_map"sv, rthis.sub_map_), _rc);
        _rr(make<7>("callback"sv, [](auto&&, Ctx&) { cout << "Callback called!" << endl; }), _rc);
        _rr(make<8>("ioss"sv, rthis.ioss_), _rc);
        _rr(make<9>("oss"sv, rthis.oss_), _rc);
        _rr(make<9>("iss"sv, rthis.iss_).set_progress([](std::istream&, size_t, bool, Ctx&) { cout << "Progress called" << endl; }), _rc);
        _rr(make<10>("bool_vec"sv, rthis.bool_vec_), _rc);
        _rr(make<11>("sub_arr"sv, rthis.sub_arr_), _rc);
        _rr(make<12>("tuple"sv, rthis.tuple_), _rc);
        _rr(make<13>("subw_ptr"sv, rthis.subw_ptr_), _rc);
        _rr(make<14>("bits"sv, rthis.bits_), _rc);
        _rr(make<15>("subw_opt"sv, rthis.subw_opt_), _rc);
        _rr(make<16>("null_str_opt"sv, rthis.null_str_opt_), _rc);
        _rr(make<17>("uint8_vec_opt"sv, rthis.uint8_vec_opt_), _rc);
        _rr(make<18>("bool_opt"sv, rthis.bool_opt_), _rc);
        _rr(make<19>("bool_ptr"sv, rthis.bool_ptr_), _rc);
        _rr(make<20>("str_opt"sv, rthis.str_opt_), _rc);
        _rr(make<21>("variant_vec"sv, rthis.variant_vec_), _rc);
        _rr(make<22>("number"sv, rthis.number_), _rc);
        _rr(make<23>("numbers"sv, rthis.numbers_), _rc);
    }
};

namespace detail {

// View over a fixed-size AnsiChar field, trimmed at the first NUL (the wire
// representation zero-pads unused trailing bytes).
template <std::size_t N>
[[nodiscard]] constexpr std::string_view to_string_view(const std::array<char, N>& buf) noexcept
{
    std::size_t len = 0;
    while (len < N && buf[len] != '\0') {
        ++len;
    }
    return {buf.data(), len};
}

// Copy value into a fixed-size AnsiChar field, truncating to N and zero-filling
// the remainder.
template <std::size_t N>
constexpr void assign(std::array<char, N>& buf, std::string_view value) noexcept
{
    const std::size_t len  = std::min(value.size(), N);
    auto              tail = std::copy_n(value.begin(), len, buf.begin());
    std::fill(tail, buf.end(), '\0');
}

} // namespace detail

using ElementId  = std::uint32_t;
using Price      = std::int64_t;
using Quantity   = std::uint64_t;
using OrderCount = std::uint16_t;

inline constexpr std::size_t kMaxBboLevels = 5;

#pragma pack(push, 1)

class PriceLevel {
public:
    static constexpr std::size_t kEncodedSize = 31;

    constexpr PriceLevel() noexcept = default;

    [[nodiscard]] constexpr bool is_encrypted() const noexcept { return isEncrypted_; }
    constexpr void               set_is_encrypted(bool value) noexcept { isEncrypted_ = value; }

    [[nodiscard]] constexpr std::uint64_t encryption_offset() const noexcept { return encryptionOffset_; }
    constexpr void                        set_encryption_offset(std::uint64_t value) noexcept { encryptionOffset_ = value; }

    // Reference to an EncryptionKey message.
    [[nodiscard]] constexpr ElementId encryption_key_id() const noexcept { return encryptionKeyId_; }
    constexpr void                    set_encryption_key_id(ElementId value) noexcept { encryptionKeyId_ = value; }

    // Price level in currency units. Encrypted on the wire when
    // is_encrypted() is true; decrypt using encryption_key_id() first.
    [[nodiscard]] constexpr Price price() const noexcept { return price_; }
    constexpr void                set_price(Price value) noexcept { price_ = value; }

    // Quantity of securities available at this price level (encrypted).
    [[nodiscard]] constexpr Quantity quantity() const noexcept { return quantity_; }
    constexpr void                   set_quantity(Quantity value) noexcept { quantity_ = value; }

    // Number of open orders at this price level (encrypted).
    [[nodiscard]] constexpr OrderCount order_count() const noexcept { return orderCount_; }
    constexpr void                     set_order_count(OrderCount value) noexcept { orderCount_ = value; }

    template <typename T, typename Reflector, typename Ctx>
    void solidReflectV2(this T& rthis, Reflector&& _rr, Ctx& _rc)
    {
        _rr(SFR_V2_MAKEF(1, rthis, is_encrypted), _rc);
        _rr(SFR_V2_MAKEF(2, rthis, encryption_offset), _rc);
        _rr(SFR_V2_MAKEF(3, rthis, encryption_key_id), _rc);
        _rr(SFR_V2_MAKEF(4, rthis, price), _rc);
        _rr(SFR_V2_MAKEF(5, rthis, quantity), _rc);
        _rr(SFR_V2_MAKEF(6, rthis, order_count), _rc);
    }

private:
    bool          isEncrypted_{};
    std::uint64_t encryptionOffset_{};
    ElementId     encryptionKeyId_{};
    Price         price_{};
    Quantity      quantity_{};
    OrderCount    orderCount_{};
};

class Two {
    static constexpr std::size_t kSecretKeySize   = 8;
    static constexpr std::size_t kDescriptionSize = 64;

    using Levels = std::array<PriceLevel, kMaxBboLevels>;

    std::uint32_t                            v1_{};
    std::array<std::uint8_t, kSecretKeySize> secretKey_{};
    std::array<char, kDescriptionSize>       description_{};
    std::uint8_t                             maxDepth_{};
    Levels                                   buy_{};
    Levels                                   sell_{};

public:
    [[nodiscard]] constexpr auto v1() const noexcept { return v1_; }
    constexpr void               set_v1(std::uint32_t _v1) noexcept { v1_ = _v1; }

    [[nodiscard]] constexpr std::span<const std::uint8_t, kSecretKeySize> secret_key() const noexcept { return secretKey_; }
    [[nodiscard]] constexpr std::span<std::uint8_t, kSecretKeySize>       secret_key() noexcept { return secretKey_; }
    constexpr void                                                        set_secret_key(std::span<const std::uint8_t> value) noexcept
    {
        std::ranges::copy_n(value.begin(), std::min(value.size(), secretKey_.size()), secretKey_.begin());
    }
    constexpr void set_secret_key(std::array<std::uint8_t, kSecretKeySize> const& value) noexcept
    {
        set_secret_key(std::span{value});
    }

    [[nodiscard]] constexpr std::string_view description() const noexcept { return detail::to_string_view(description_); }
    constexpr void                           set_description(std::string_view value) noexcept { detail::assign(description_, value); }

    // Number of BBO levels actually broadcast in this message (<= kMaxBboLevels).
    [[nodiscard]] constexpr std::uint8_t max_depth() const noexcept { return maxDepth_; }
    constexpr void                       set_max_depth(std::uint8_t value) noexcept { maxDepth_ = value; }

    // Full fixed-size buy-side level array, as laid out on the wire.
    [[nodiscard]] constexpr std::span<const PriceLevel, kMaxBboLevels> buy() const noexcept { return buy_; }
    [[nodiscard]] constexpr std::span<PriceLevel, kMaxBboLevels>       buy() noexcept { return buy_; }
    constexpr void                                                     set_buy(std::span<const PriceLevel> levels) noexcept
    {
        std::ranges::copy_n(levels.begin(), std::min(levels.size(), buy_.size()), buy_.begin());
    }
    [[nodiscard]] constexpr std::span<const PriceLevel> active_buy() const noexcept
    {
        return std::span<const PriceLevel>(buy_).first(std::min<std::size_t>(maxDepth_, kMaxBboLevels));
    }

    [[nodiscard]] constexpr std::span<const PriceLevel, kMaxBboLevels> sell() const noexcept { return sell_; }
    [[nodiscard]] constexpr std::span<PriceLevel, kMaxBboLevels>       sell() noexcept { return sell_; }
    constexpr void                                                     set_sell(std::span<const PriceLevel> levels) noexcept
    {
        std::ranges::copy_n(levels.begin(), std::min(levels.size(), buy_.size()), buy_.begin());
    }
    [[nodiscard]] constexpr std::span<const PriceLevel> active_sell() const noexcept
    {
        return std::span<const PriceLevel>(buy_).first(std::min<std::size_t>(maxDepth_, kMaxBboLevels));
    }

    void init()
    {
        set_v1(1111);
        set_secret_key({1, 2, 3, 4, 5, 6, 7, 8});
        set_description("some description"sv);
        set_max_depth(4);
        for (size_t i{0}; i < max_depth(); ++i) {
            buy()[i].set_is_encrypted(false);
            buy()[i].set_encryption_key_id(i);
            buy()[i].set_encryption_offset(i);
            buy()[i].set_order_count(i);
            buy()[i].set_quantity(i * 100);

            sell()[i].set_is_encrypted(true);
            sell()[i].set_encryption_key_id(4 - i);
            sell()[i].set_encryption_offset(4 - i);
            sell()[i].set_order_count(4 - i);
            sell()[i].set_quantity(i * 200);
        }
    }

    enum class FieldE {
        v1,
        secret_key,
        description,
        max_depth,
        buy,
        sell
    };
    template <typename T, typename Reflector, typename Ctx>
    void solidReflectV2(this T& rthis, Reflector&& _rr, Ctx& _rc)
    {
        _rr(SFR_V2_MAKEF(FieldE::v1, rthis, v1), _rc);
        _rr(SFR_V2_MAKEF(FieldE::secret_key, rthis, secret_key), _rc);
        _rr(SFR_V2_MAKEF(FieldE::description, rthis, description), _rc);
        _rr(SFR_V2_MAKEF(FieldE::max_depth, rthis, max_depth), _rc);
        _rr(SFR_V2_MAKEF(FieldE::buy, rthis, buy).with_max_size(rthis.max_depth()), _rc);
        _rr(SFR_V2_MAKEF(FieldE::sell, rthis, sell).with_max_size(rthis.max_depth()), _rc);
    }
};

#pragma pack(pop)

} // namespace alpha

int test_json(int /*argc*/, char* /*argv*/[])
{
    size_t item_count{1000};
    size_t repeat_count{3};

    struct Context {
    } ctx;

    to_json(cout, 1024, ctx) << endl;
    std::string json_str;

    {
        alpha::One one{
            true, 100, "something",
            {.a_ = 54321, .b_ = "sub string"},
            {{.a_ = 1, .b_ = "1"}, {.a_ = 2, .b_ = "2"}, {.a_ = 3, .b_ = "3"}},
            {{"alpha", {.a_ = 1, .b_ = "1"}}, {"betha", {.a_ = 2, .b_ = "2"}}, {"gamma", {.a_ = 3, .b_ = "3"}}},
            {{{.a_ = 1, .b_ = "1"}, {.a_ = 2, .b_ = "2"}, {.a_ = 3, .b_ = "3"}}}};

        one.tuple_              = {"tuple_string", 10, {.a_ = 54321, .b_ = "sub string"}};
        one.subw_ptr_           = std::make_unique<alpha::SubWrap>();
        one.subw_ptr_->sub_lst_ = {{.a_ = 1, .b_ = "1"}, {.a_ = 2, .b_ = "2"}, {.a_ = 3, .b_ = "3"}};
        one.subw_ptr_->sub_ptr_ = std::make_shared<alpha::Sub>(121, "sub shared ptr");
        one.subw_opt_.emplace();
        one.subw_opt_->sub_lst_ = {{.a_ = 1, .b_ = "1"}, {.a_ = 2, .b_ = "2"}, {.a_ = 3, .b_ = "3"}};
        one.subw_opt_->sub_ptr_ = std::make_shared<alpha::Sub>(121, "sub shared ptr");

        one.bits_.set();
        one.bits_.reset(7);
        one.bits_.reset(15);
        one.bits_.reset(23);
        one.uint8_vec_opt_ = std::vector<std::uint8_t>{1, 2, 3, 4, 5, 6, 7, 8, 9};
        one.bool_ptr_      = std::make_shared<bool>(true);
        one.bool_opt_      = true;
        one.str_opt_.emplace("null");
        one.variant_vec_.emplace_back(std::monostate{});
        one.variant_vec_.emplace_back(alpha::Sub{.a_ = 100, .b_ = "100"});
        one.variant_vec_.emplace_back("some string");
        one.variant_vec_.emplace_back(std::vector<alpha::Sub>{{.a_ = 1000, .b_ = "1000"}, {.a_ = 2000, .b_ = "2000"}, {.a_ = 3000, .b_ = "3000"}});

        to_json_str(json_str, one, ctx);
        cout << "to_json: " << json_str << endl;

        solid::reflection::v2::reflect_at<3>(
            one,
            [](auto&& _field, Context&) {
                static_assert(_field.type_id == solid::reflection::v2::TypeIdE::String);
                _field.set("something else");
            },
            ctx);
        json_str.clear();
        to_json_str(json_str, one, ctx);
        cout << "after modify:" << json_str << endl;
    }
    {
        alpha::One one;
        if (auto res = from_json(json_str, one, ctx); res) {
            std::string str;
            to_json_str(str, one, ctx);
            cout << "from_json: " << str << endl;
            if (str != json_str || str.empty()) {
                cout << "ERROR: One round-trip mismatch" << endl;
                return 1;
            }
        } else {
            cout << "parse error: " << res.error().message() << endl;
            return 1;
        }
    }

    {
        alpha::Two two;
        two.init();
        json_str.clear();

        solid::reflection::v2::reflect_at<alpha::Two::FieldE::description>(
            two,
            [](auto&& _field, Context&) {
                static_assert(_field.type_id == solid::reflection::v2::TypeIdE::String);
                solid_check(_field.get() == "some description"sv);
                _field.set("some other description"sv);
            },
            ctx);
        to_json_str(json_str, two, ctx);
        cout << "two: to_json: " << json_str << endl;
    }

    {
        alpha::Two two;
        if (auto res = from_json(json_str, two, ctx); res) {
            std::string str;
            to_json_str(str, two, ctx);
            cout << "two: from_json: " << str << endl;
            if (str != json_str || str.empty()) {
                cout << "ERROR: Two round-trip mismatch" << endl;
                return 1;
            }
        } else {
            cout << "parse error: " << res.error().message() << endl;
            return 1;
        }
    }

    {
        alpha::Two         two;
        std::istringstream iss{json_str};
        if (auto res = from_json(iss, two, ctx); res) {
            std::string str;
            to_json_str(str, two, ctx);
            cout << "two: from_json(istream): " << str << endl;
            if (str != json_str || str.empty()) {
                cout << "ERROR: Two istream round-trip mismatch" << endl;
                return 1;
            }
        } else {
            cout << "parse error: " << res.error().message() << endl;
            return 1;
        }
    }

    {
        std::deque<alpha::One> one_dq;
        for (size_t i{0}; i < 5; ++i) {
            one_dq.push_back(
                {(i % 1) == 0, static_cast<std::uint32_t>(100 + i), "something",
                    {.a_ = 54321 + i, .b_ = "sub string"},
                    {{.a_ = i + 1, .b_ = "1"}, {.a_ = i + 2, .b_ = "2"}, {.a_ = i + 3, .b_ = "3"}},
                    {{"alpha", {.a_ = i + 1, .b_ = "1"}}, {"betha", {.a_ = i + 2, .b_ = "2"}}, {"gamma", {.a_ = i + 3, .b_ = "3"}}},
                    {{{.a_ = i + 1, .b_ = "1"}, {.a_ = i + 2, .b_ = "2"}, {.a_ = i + 3, .b_ = "3"}}}});

            one_dq.back().tuple_              = {"tuple_string", 10, {.a_ = 54321, .b_ = "sub string"}};
            one_dq.back().subw_ptr_           = std::make_unique<alpha::SubWrap>();
            one_dq.back().subw_ptr_->sub_lst_ = {{.a_ = i + 1, .b_ = "1"}, {.a_ = i + 2, .b_ = "2"}, {.a_ = i + 3, .b_ = "3"}};
            one_dq.back().subw_ptr_->sub_ptr_ = std::make_shared<alpha::Sub>(121, "sub shared ptr");
            one_dq.back().subw_opt_.emplace();
            one_dq.back().subw_opt_->sub_lst_ = {{.a_ = 1, .b_ = "1"}, {.a_ = 2, .b_ = "2"}, {.a_ = 3, .b_ = "3"}};
            one_dq.back().subw_opt_->sub_ptr_ = std::make_shared<alpha::Sub>(121, "sub shared ptr");
            one_dq.back().bits_.set();
            one_dq.back().bits_.reset(7);
            one_dq.back().bits_.reset(15);
            one_dq.back().bits_.reset(23);
        }
        {
            std::string dq_json;
            to_json_str(dq_json, one_dq, ctx);
            cout << dq_json << endl;
        }
    }
    { // performance test
        std::vector<alpha::One> one_dq;
        one_dq.reserve(item_count);
        for (size_t i{0}; i < item_count; ++i) {
            one_dq.push_back(
                {(i % 1) == 0, static_cast<std::uint32_t>(100 + i), "something",
                    {.a_ = 54321 + i, .b_ = "sub string"},
                    {{.a_ = i + 1, .b_ = "1"}, {.a_ = i + 2, .b_ = "2"}, {.a_ = i + 3, .b_ = "3"}},
                    {{"alpha", {.a_ = i + 1, .b_ = "1"}}, {"betha", {.a_ = i + 2, .b_ = "2"}}, {"gamma", {.a_ = i + 3, .b_ = "3"}}},
                    {{{.a_ = i + 1, .b_ = "1"}, {.a_ = i + 2, .b_ = "2"}, {.a_ = i + 3, .b_ = "3"}}}});

            one_dq.back().tuple_              = {"tuple_string", 10, {.a_ = 54321, .b_ = "sub string"}};
            one_dq.back().subw_ptr_           = std::make_unique<alpha::SubWrap>();
            one_dq.back().subw_ptr_->sub_lst_ = {{.a_ = i + 1, .b_ = "1"}, {.a_ = i + 2, .b_ = "2"}, {.a_ = i + 3, .b_ = "3"}};
            one_dq.back().subw_ptr_->sub_ptr_ = std::make_shared<alpha::Sub>(121, "sub shared ptr");
            one_dq.back().subw_opt_.emplace();
            one_dq.back().subw_opt_->sub_lst_ = {{.a_ = 1, .b_ = "1"}, {.a_ = 2, .b_ = "2"}, {.a_ = 3, .b_ = "3"}};
            one_dq.back().subw_opt_->sub_ptr_ = std::make_shared<alpha::Sub>(121, "sub shared ptr");
            one_dq.back().bits_.set();
            one_dq.back().bits_.reset(7);
            one_dq.back().bits_.reset(15);
            one_dq.back().bits_.reset(23);
        }

        using clock = std::chrono::steady_clock;
        struct Durations {
            int64_t to_us;
            int64_t from_us;
            int64_t from_is_us;
        };
        std::vector<Durations> durations_us;
        durations_us.reserve(repeat_count);

        std::string json_buf;
        for (size_t i{0}; i < repeat_count; ++i) {
            json_buf.clear();
            auto t0 = clock::now();
            to_json_str(json_buf, one_dq, ctx);
            auto                    t1 = clock::now();
            std::vector<alpha::One> tmp_dq(item_count);
            auto                    t2 = clock::now();
            if (auto res = from_json(json_buf, tmp_dq, ctx); not res) {
                cout << "parse error: " << res.error().message() << endl;
                return 1;
            }
            auto                    t3 = clock::now();
            std::vector<alpha::One> tmp_is_dq(item_count);
            std::istringstream      json_is{json_buf};
            auto                    t4 = clock::now();
            if (auto res = from_json(json_is, tmp_is_dq, ctx); not res) {
                cout << "parse error (istream): " << res.error().message() << endl;
                return 1;
            }
            auto t5 = clock::now();
            durations_us.emplace_back(
                std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count(),
                std::chrono::duration_cast<std::chrono::microseconds>(t3 - t2).count(),
                std::chrono::duration_cast<std::chrono::microseconds>(t5 - t4).count());
            cout << "line length: " << json_buf.size() << endl;
        }

        int64_t total_to = 0, total_from = 0, total_from_is = 0;
        for (auto const& [to, from, from_is] : durations_us) {
            cout << "to: " << to << "us " << "from: " << from << "us " << "from(istream): " << from_is << "us\n";
            total_to += to;
            total_from += from;
            total_from_is += from_is;
        }

        cout << "total: to_json=" << total_to / 1000.0 << "ms, from_json=" << total_from / 1000.0
             << "ms, from_json(istream)=" << total_from_is / 1000.0 << "ms\n";
        cout << flush;
    }
    return 0;
}
