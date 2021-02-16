#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <vector>

#include "solid/serialization/v3/serialization.hpp"

using namespace solid;
using namespace std;

struct Key {
    virtual ~Key() {}
    virtual void print(ostream& _ros) const = 0;
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

    SOLID_REFLECT_V1(_s, _rthis, _rctx)
    {
        _s.add(_rthis.first, _rctx, 1, "first").add(_rthis.second, _rctx, 2, "second");
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

    SOLID_REFLECT_V1(_s, _rthis, _rctx)
    {
        _s.add(_rthis.keys, _rctx, 1, "keys");
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

    SOLID_REFLECT_V1(_s, _rthis, _rctx)
    {
        _s.add(_rthis.v, _rctx, 1, "v");
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

    SOLID_REFLECT_V1(_s, _rthis, _rctx)
    {
        _s.add(_rthis.v, _rctx, 1, "v");
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

    SOLID_REFLECT_V1(_s, _rthis, _rctx)
    {
        _s.add(_rthis.key, _rctx, 1, "key");
    }

    void print(ostream& _ros) const
    {
        if (key) {
            key->print(_ros);
        }
    }
};

int test_polymorphic(int /*argc*/, char* /*argv*/[])
{

    solid::log_start(std::cerr, {".*:EWX"});
    
    using ContextT = solid::EmptyType;
    using SerializerT = serialization::v3::binary::Serializer<reflection::metadata::Variant, decltype(reflection::metadata::factory), ContextT, uint8_t>;
    using DeserializerT = serialization::v3::binary::Deserializer<reflection::metadata::Variant, decltype(reflection::metadata::factory), ContextT, uint8_t>;
    
    const reflection::TypeMap<SerializerT, DeserializerT> key_type_map{
        [](auto &_rmap){
            _rmap.template registerType<Command>(0, 1, "Command");
            _rmap.template registerType<OrKey, Key>(0, 2, "OrKey");
            _rmap.template registerType<OrVecKey, Key>(0, 3, "OrVec");
            _rmap.template registerType<IntKey, Key>(0, 4, "IntKey");
            
        }
    };
    
    string   check_data;
    string   test_data;

    { //serialization
        ContextT    ctx;
        SerializerT ser{reflection::metadata::factory, key_type_map};
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

        ser.add(cmd_ptr, ctx, 1, "cmd_ptr");

        while ((rv = ser.run(buf, bufcp, ctx)) > 0) {
            test_data.append(buf, rv);
        }
    }
    { //deserialization
        ContextT      ctx;
        DeserializerT des{reflection::metadata::factory, key_type_map};

        shared_ptr<Command> cmd_ptr;

        des.add(cmd_ptr, ctx, 1, "cmd_ptr");

        long rv = des.run(test_data.data(), test_data.size(), ctx);

        solid_check(rv == static_cast<int>(test_data.size()));

        ostringstream oss;

        cmd_ptr->print(oss);

        cout << "check_data = " << oss.str() << endl;

        solid_check(check_data == oss.str());
    }
    return 0;
}
