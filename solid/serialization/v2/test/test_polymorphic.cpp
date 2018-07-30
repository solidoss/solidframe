#include "solid/serialization/v2/serialization.hpp"
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <vector>

using namespace solid;
using namespace std;

struct Key {
    virtual ~Key() {}
    virtual void print(ostream& _ros) const = 0;
};

struct Context {
};

struct TypeData {
};

struct OrKey : Key {
    shared_ptr<Key> first;
    shared_ptr<Key> second;

    OrKey() {}

    template <class K1, class K2>
    OrKey(shared_ptr<K1>&& _k1, shared_ptr<K2>&& _k2)
        : first(std::move(static_pointer_cast<Key>(_k1)))
        , second(std::move(static_pointer_cast<Key>(_k2)))
    {
    }

    SOLID_SERIALIZE_CONTEXT_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.first, _rctx, "first").add(_rthis.second, _rctx, "second");
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

    SOLID_SERIALIZE_CONTEXT_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.keys, _rctx, "keys");
    }

    void print(ostream& _ros) const override
    {
        _ros << '(';
        for (auto const& key_ptr : keys) {
            key_ptr->print(_ros);
            _ros << ',';
        }
        _ros << ')';
    }
};

struct IntKey : Key {
    int v;

    IntKey() {}
    IntKey(int _v)
        : v(_v)
    {
    }

    SOLID_SERIALIZE_CONTEXT_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.v, _rctx, "v");
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

    SOLID_SERIALIZE_CONTEXT_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.v, _rctx, "v");
    }

    void print(ostream& _ros) const override
    {
        _ros << '\"' << v << '\"';
    }
};

struct Command {
    shared_ptr<Key> key;

    Command() {}

    template <class K>
    Command(shared_ptr<K>&& _k)
        : key(static_pointer_cast<Key>(std::move(_k)))
    {
    }

    SOLID_SERIALIZE_CONTEXT_V2(_s, _rthis, _rctx, _name)
    {
        _s.add(_rthis.key, _rctx, "key");
    }

    void print(ostream& _ros) const
    {
        if (key) {
            key->print(_ros);
        }
    }
};

int test_polymorphic(int argc, char* argv[])
{

    solid::log_start(std::cerr, {".*:EW"});

    using TypeMapT      = serialization::TypeMap<uint8_t, Context, serialization::binary::Serializer, serialization::binary::Deserializer, TypeData>;
    using SerializerT   = TypeMapT::SerializerT;
    using DeserializerT = TypeMapT::DeserializerT;

    string   check_data;
    string   test_data;
    TypeMapT typemap;

    typemap.null(0);
    typemap.registerType<Command>(1);
    typemap.registerType<OrKey>(2);
    typemap.registerType<OrVecKey>(3);
    typemap.registerType<IntKey>(4);
    typemap.registerType<StringKey>(5);
    typemap.registerCast<OrKey, Key>();
    typemap.registerCast<OrVecKey, Key>();
    typemap.registerCast<IntKey, Key>();
    typemap.registerCast<StringKey, Key>();

    { //serialization
        Context     ctx;
        SerializerT ser   = typemap.createSerializer();
        const int   bufcp = 64;
        char        buf[bufcp];
        long        rv;

        shared_ptr<Command> cmd_ptr = std::make_shared<Command>(
            std::make_shared<OrVecKey>(
                std::make_shared<OrKey>(
                    std::make_shared<IntKey>(10),
                    std::make_shared<StringKey>("10")),
                std::make_shared<IntKey>(11),
                std::make_shared<StringKey>("11"),
                std::make_shared<OrKey>(
                    std::make_shared<StringKey>("13"),
                    std::make_shared<OrKey>(
                        std::make_shared<StringKey>("14"),
                        std::make_shared<IntKey>(14)))));

        ostringstream oss;

        cmd_ptr->print(oss);

        check_data = oss.str();

        cout << "check_data = " << check_data << endl;

        ser.add(cmd_ptr, ctx, "cmd_ptr");

        while ((rv = ser.run(buf, bufcp, ctx)) > 0) {
            test_data.append(buf, rv);
        }
    }
    { //deserialization
        Context       ctx;
        DeserializerT des = typemap.createDeserializer();

        shared_ptr<Command> cmd_ptr;

        des.add(cmd_ptr, ctx, "cmd_ptr");

        long rv = des.run(test_data.data(), test_data.size(), ctx);

        solid_check(rv == static_cast<int>(test_data.size()));

        ostringstream oss;

        cmd_ptr->print(oss);

        cout << "check_data = " << oss.str() << endl;

        solid_check(check_data == oss.str());
    }
    return 0;
}
