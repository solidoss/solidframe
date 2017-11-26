#include "solid/serialization/binary.hpp"
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

    template <class S>
    void solidSerialize(S& _s)
    {
        _s.push(second, "second").push(first, "first");
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

    template <class S>
    void solidSerialize(S& _s)
    {
        _s.pushContainer(keys, "keys");
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

    template <class S>
    void solidSerialize(S& _s)
    {
        _s.push(v, "v");
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

    template <class S>
    void solidSerialize(S& _s)
    {
        _s.push(v, "v");
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

    template <class S>
    void solidSerialize(S& _s)
    {
        _s.push(key, "key");
    }

    void print(ostream& _ros) const
    {
        if (key) {
            key->print(_ros);
        }
    }
};

using SerializerT   = serialization::binary::Serializer<void>;
using DeserializerT = serialization::binary::Deserializer<void>;
using TypeIdMapT    = serialization::TypeIdMap<SerializerT, DeserializerT>;

int test_polymorphic(int argc, char* argv[])
{

#ifdef SOLID_HAS_DEBUG
    Debug::the().levelMask("ew");
    Debug::the().moduleMask("all");
    Debug::the().initStdErr(false, nullptr);
#endif

    string     check_data;
    string     test_data;
    TypeIdMapT typemap;

    typemap.registerType<Command>(0 /*protocol ID*/);
    typemap.registerType<OrKey>(0);
    typemap.registerType<OrVecKey>(0);
    typemap.registerType<IntKey>(0);
    typemap.registerType<StringKey>(0);
    typemap.registerCast<OrKey, Key>();
    typemap.registerCast<OrVecKey, Key>();
    typemap.registerCast<IntKey, Key>();
    typemap.registerCast<StringKey, Key>();

    { //serialization
        SerializerT ser(&typemap);
        const int   bufcp = 64;
        char        buf[bufcp];
        int         rv;

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

        ser.push(cmd_ptr, "cmd_ptr");

        while ((rv = ser.run(buf, bufcp)) > 0) {
            test_data.append(buf, rv);
        }
    }
    { //deserialization
        DeserializerT des(&typemap);

        shared_ptr<Command> cmd_ptr;

        des.push(cmd_ptr, "cmd_ptr");

        int rv = des.run(test_data.data(), test_data.size());

        SOLID_CHECK(rv == static_cast<int>(test_data.size()));

        ostringstream oss;

        cmd_ptr->print(oss);

        cout << "check_data = " << oss.str() << endl;

        SOLID_CHECK(check_data == oss.str());
    }
    return 0;
}
