#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "solid/serialization/v4/serialization.hpp"
#include "solid/system/exception.hpp"

using namespace solid;
using namespace std;
using namespace std::literals;

namespace {

using reflection::v2::TypeCategory;
using reflection::v2::TypeId;
using reflection::v2::TypeName;

struct Context {
    size_t created = 0;
};

struct Key {
    virtual ~Key() {}
    virtual void print(ostream& _ros) const = 0;
};

struct OrKey : Key {
    shared_ptr<Key> first;
    shared_ptr<Key> second;

    OrKey() {}

    template <class K1, class K2>
    OrKey(shared_ptr<K1>&& _k1, shared_ptr<K2>&& _k2)
        : first(std::move(_k1))
        , second(std::move(_k2))
    {
    }

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("first"sv, rthis.first), _rc);
        _rr(id<2>, make("second"sv, rthis.second), _rc);
    }

    void print(ostream& _ros) const override
    {
        _ros << '(';
        if (first) {
            first->print(_ros);
        }
        _ros << ',';
        if (second) {
            second->print(_ros);
        }
        _ros << ')';
    }
};

struct OrVecKey : Key {
    vector<shared_ptr<Key>> keys;

    OrVecKey() {}

    template <class... Args>
    OrVecKey(shared_ptr<Args>&&... _args)
        : keys{std::move(_args)...}
    {
    }

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("keys"sv, rthis.keys), _rc);
    }

    void print(ostream& _ros) const override
    {
        _ros << '[';
        for (auto const& key_ptr : keys) {
            if (key_ptr) {
                key_ptr->print(_ros);
            } else {
                _ros << "null";
            }
            _ros << ',';
        }
        _ros << ']';
    }
};

struct IntKey : Key {
    int v = -1;

    IntKey() {}
    IntKey(int _v)
        : v(_v)
    {
    }

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("v"sv, rthis.v), _rc);
    }

    void print(ostream& _ros) const override
    {
        _ros << v;
    }
};

struct StringKey : Key {
    string v;

    StringKey() {}
    StringKey(string&& _v)
        : v(std::move(_v))
    {
    }

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("v"sv, rthis.v), _rc);
    }

    void print(ostream& _ros) const override
    {
        _ros << '\"' << v << '\"';
    }
};

// Second base: reachable as Named through register_cast.
struct Named {
    virtual ~Named() {}
    virtual string name() const = 0;
};

struct NamedIntKey : IntKey, Named {
    string label;

    NamedIntKey() {}
    NamedIntKey(int _v, string _label)
        : IntKey(_v)
        , label(std::move(_label))
    {
    }

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("v"sv, rthis.v), _rc);
        _rr(id<2>, make("label"sv, rthis.label), _rc);
    }

    string name() const override
    {
        return label;
    }

    void print(ostream& _ros) const override
    {
        _ros << label << ':' << v;
    }
};

// Created through a custom create function
struct Counted : Key {
    uint32_t value  = 0;
    size_t   serial = 0;

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("value"sv, rthis.value), _rc);
    }

    void print(ostream& _ros) const override
    {
        _ros << "counted " << value;
    }
};

struct Command {
    shared_ptr<Key>   key;
    unique_ptr<Key>   ukey;
    unique_ptr<Key>   null_key;
    shared_ptr<Named> named;
    shared_ptr<Key>   counted;

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("key"sv, rthis.key), _rc);
        _rr(id<2>, make("ukey"sv, rthis.ukey), _rc);
        _rr(id<3>, make("null_key"sv, rthis.null_key), _rc);
        _rr(id<4>, make("named"sv, rthis.named), _rc);
        _rr(id<5>, make("counted"sv, rthis.counted), _rc);
    }

    void print(ostream& _ros) const
    {
        if (key) {
            key->print(_ros);
        }
        _ros << '|';
        if (ukey) {
            ukey->print(_ros);
        }
        _ros << '|' << (null_key ? "set" : "null") << '|' << (named ? named->name() : "null") << '|';
        if (counted) {
            counted->print(_ros);
        }
    }
};

template <class TypeIdT>
struct Fixture {
    using SerializerT   = serialization::v4::binary::Serializer<Context, TypeIdT>;
    using DeserializerT = serialization::v4::binary::Deserializer<Context, TypeIdT>;
    using TypeMapT      = reflection::v2::TypeMap<SerializerT, DeserializerT>;

    // only a pair TypeId carries the category on the wire
    static constexpr TypeCategory category(const size_t _category)
    {
        return TypeCategory{serialization::v4::binary::detail::is_std_pair_v<TypeIdT> ? _category : 0};
    }

    static TypeMapT const& typeMap()
    {
        static const TypeMapT type_map{[](auto& _rmap) {
            _rmap.template register_type<Command>(TypeId{1}, TypeName{"Command"});
            _rmap.template register_type<OrKey, Key>(TypeId{2}, TypeName{"OrKey"});
            _rmap.template register_type<OrVecKey, Key>(TypeId{3}, TypeName{"OrVec"});
            _rmap.template register_type<IntKey, Key>(category(1), TypeId{4}, TypeName{"IntKey"});
            _rmap.template register_type<StringKey, Key>(category(1), TypeId{5000}, TypeName{"StringKey"});
            _rmap.template register_type<NamedIntKey, Key>(category(1), TypeId{6}, TypeName{"NamedIntKey"});
            _rmap.template register_cast<NamedIntKey, Named>();
            _rmap.template register_type<Counted, Key>(category(2), TypeId{7}, TypeName{"Counted"}, [](Context& _rctx, auto& _rptr) {
                using PtrT = std::decay_t<decltype(_rptr)>;
                if constexpr (std::is_same_v<PtrT, std::shared_ptr<Counted>>) {
                    _rptr         = std::make_shared<Counted>();
                    _rptr->serial = ++_rctx.created;
                }
            });
        }};
        return type_map;
    }

    static string serialize(shared_ptr<Command> const& _cmd, const size_t _chunk)
    {
        Context      ctx;
        SerializerT  ser{typeMap()};
        vector<char> buf(_chunk);
        string       out;

        ser.add(_cmd, ctx); // added outside run: everything is scheduled
        for (ptrdiff_t rv = ser.run(buf.data(), buf.size(), ctx); rv > 0; rv = ser.run(buf.data(), buf.size(), ctx)) {
            out.append(buf.data(), static_cast<size_t>(rv));
        }
        solid_check(!ser.error(), ser.error().message());
        solid_check(ser.empty());
        return out;
    }

    static shared_ptr<Command> deserialize(string const& _data, const size_t _chunk, Context& _rctx)
    {
        DeserializerT       des{typeMap()};
        shared_ptr<Command> cmd;
        size_t              offset = 0;

        des.add(cmd, _rctx);
        while (offset < _data.size()) {
            const size_t    sz = std::min(_chunk, _data.size() - offset);
            const ptrdiff_t rv = des.run(_data.data() + offset, sz, _rctx);
            solid_check(rv >= 0, des.error().message());
            offset += static_cast<size_t>(rv);
            if (des.empty()) {
                break;
            }
        }
        solid_check(des.empty() && offset == _data.size());
        return cmd;
    }

    static void run(const char* _name)
    {
        auto cmd = std::make_shared<Command>();
        cmd->key = std::make_shared<OrVecKey>(
            std::make_shared<OrKey>(
                std::make_shared<IntKey>(10),
                std::make_shared<StringKey>("10")),
            std::make_shared<IntKey>(11),
            std::make_shared<StringKey>("11"),
            std::shared_ptr<Key>{},
            std::make_shared<NamedIntKey>(12, "twelve"),
            std::make_shared<OrKey>(
                std::make_shared<StringKey>("13"),
                std::make_shared<OrKey>(
                    std::make_shared<StringKey>("14"),
                    std::make_shared<IntKey>(14))));
        cmd->ukey      = std::make_unique<StringKey>("unique");
        cmd->named     = std::make_shared<NamedIntKey>(15, "fifteen");
        auto counted   = std::make_shared<Counted>();
        counted->value = 77;
        cmd->counted   = counted;

        ostringstream oss;
        cmd->print(oss);
        const string check_data = oss.str();
        cout << _name << ": " << check_data << endl;

        const string reference = serialize(cmd, 4096);
        for (size_t chunk = 1; chunk < 64; ++chunk) {
            solid_check(serialize(cmd, chunk) == reference);
        }
        for (size_t chunk = 1; chunk <= reference.size(); chunk += (chunk < 64 ? 1 : 50)) {
            Context       ctx;
            auto          cmd_copy = deserialize(reference, chunk, ctx);
            ostringstream oss_copy;
            cmd_copy->print(oss_copy);
            solid_check(oss_copy.str() == check_data, oss_copy.str());
            solid_check(ctx.created == 1);
            solid_check(static_cast<Counted const&>(*cmd_copy->counted).serial == 1);
            solid_check(dynamic_cast<NamedIntKey const*>(cmd_copy->named.get()) != nullptr);
        }
    }
};

struct Unregistered : Key {
    void print(ostream&) const override {}
    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto&, Reflector&&, Ctx&)
    {
    }
};

void test_errors()
{
    using FixtureT      = Fixture<uint16_t>;
    using SerializerT   = FixtureT::SerializerT;
    using DeserializerT = FixtureT::DeserializerT;

    Context ctx;
    {
        // unknown dynamic type on serialization
        shared_ptr<Key> key = std::make_shared<Unregistered>();
        SerializerT     ser{FixtureT::typeMap()};
        char            buf[64];
        solid_check(ser.run(buf, sizeof(buf), [&](SerializerT& _rs, Context& _rctx) { _rs.add(key, _rctx); }, ctx) == -1);
        solid_check(ser.error() == serialization::v4::error_unknown_type);
    }
    {
        // unknown id on deserialization
        const string    data("\x39\x30", 2); // 12345
        shared_ptr<Key> key;
        DeserializerT   des{FixtureT::typeMap()};
        solid_check(des.run(data.data(), data.size(), [&](DeserializerT& _rd, Context& _rctx) { _rd.add(key, _rctx); }, ctx) == -1);
        solid_check(des.error() == serialization::v4::error_unknown_type);
    }
    {
        // IntKey is not registered as a Named
        const string      data("\x04\x00\x01\x00\x00\x00", 6);
        shared_ptr<Named> named;
        DeserializerT     des{FixtureT::typeMap()};
        solid_check(des.run(data.data(), data.size(), [&](DeserializerT& _rd, Context& _rctx) { _rd.add(named, _rctx); }, ctx) == -1);
        solid_check(des.error() == serialization::v4::error_reflector_type);
    }
    {
        const auto& type_map = FixtureT::typeMap();
        solid_check(type_map.size() == 7);
        const auto* pentry = type_map.find(TypeCategory{0}, TypeName{"StringKey"});
        solid_check(pentry != nullptr && pentry->id() == 5000 && pentry == type_map.find(TypeCategory{0}, TypeId{5000}));
        solid_check(type_map.find(TypeCategory{1}, TypeId{5000}) == nullptr && type_map.find(TypeCategory{0}, TypeId{5001}) == nullptr);
        const StringKey sk;
        const Key&      rkey = sk;
        solid_check(type_map.find_dynamic(&rkey) == pentry);
        solid_check(type_map.index(&rkey) == pentry->index());
        solid_check(type_map.index(static_cast<Key const*>(nullptr)) == 0);
    }
}

} // namespace

int test_polymorphic(int /*argc*/, char* /*argv*/[])
{
    Fixture<uint16_t>::run("uint16_t");
    Fixture<std::pair<uint8_t, uint16_t>>::run("pair<uint8_t, uint16_t>");
    test_errors();
    return 0;
}
