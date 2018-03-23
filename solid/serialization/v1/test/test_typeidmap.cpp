#include "solid/serialization/v1/binary.hpp"
#include "solid/system/debug.hpp"
#include <sstream>

#include <iostream>
#include <string>

using namespace solid;
using namespace std;

enum struct Protocols : size_t {
    Alpha,
    Beta,
    Delta
};

struct Base {
    Base()
    {
        idbg("" << this);
    }
    virtual ~Base()
    {
        idbg("" << this);
    }
    virtual std::ostream& print(std::ostream&) const                = 0;
    virtual bool          check(std::string* _pstr = nullptr) const = 0;
};

using BasePointerT = std::shared_ptr<Base>;

//-----------------------------------------------------------------------------
//Alpha

namespace alpha {

struct Base {
    Base()
    {
        idbg("" << this);
    }
    virtual ~Base()
    {
        idbg("" << this);
    }
    virtual std::ostream& print(std::ostream&) const = 0;
    virtual bool          check() const              = 0;
};

template <size_t V>
struct Test : Base {
    std::string value;

    Test(const std::string& _value = "")
        : value(_value)
    {
    }

    std::ostream& print(std::ostream& _ros) const override
    {
        _ros << "alpha::Test<" << V << ">(" << value << ")" << endl;
        return _ros;
    }

    template <class S>
    void solidSerializeV1(S& _s)
    {
        _s.push(value, "alpha::Test::value");
    }

    bool check() const override
    {
        ostringstream oss;
        oss << "alpha::Test<" << V << ">";
        return value == oss.str();
    }
};

} //namespace alpha

//-----------------------------------------------------------------------------

//Beta

namespace beta {

struct Base : ::Base {
    uint64_t value;
    Base(const size_t _value = 0)
        : value(_value)
    {
    }
};

using BasePointerT = std::shared_ptr<Base>;

template <size_t V>
struct Test : Base {
    std::string value;

    Test(const std::string& _v = "", const size_t _value = 0)
        : Base(_value)
        , value(_v)
    {
    }

    template <class S>
    void solidSerializeV1(S& _s)
    {
        _s.push(value, "beta::Test::value");
    }

    std::ostream& print(std::ostream& _ros) const override
    {
        _ros << "beta::Test<" << V << ">(" << value << ", " << Base::value << ")" << endl;
        return _ros;
    }

    bool check(std::string* _pstr) const override
    {
        ostringstream oss;
        oss << "beta::Test<" << V << ">";
        if (_pstr) {
            return value == oss.str() && V == Base::value && *_pstr == value;
        } else {
            return value == oss.str() && V == Base::value;
        }
    }
};

template <class S, class T>
void serialize(S& _rs, T& _rt, const char* _name)
{
    _rs.pushCross(static_cast<Base&>(_rt).value, "beta::base::value");
    _rs.push(_rt, _name);
}

} //namespace beta

//-----------------------------------------------------------------------------
//Delta

namespace delta {

struct Base {
    virtual ~Base() {}
    virtual std::ostream& print(std::ostream&) const = 0;
    virtual bool          check() const              = 0;
};

template <size_t V>
struct Test : Base {
    std::string value;

    Test(const std::string& _value = "")
        : value(_value)
    {
    }

    std::ostream& print(std::ostream& _ros) const override
    {
        _ros << "delta::Test<" << V << ">(" << value << ")" << endl;
        return _ros;
    }

    bool check() const override
    {
        ostringstream oss;
        oss << "delta::Test<" << V << ">";
        return value == oss.str();
    }

    template <class S>
    void solidSerializeV1(S& _s)
    {
        _s.push(value, "delta::Test::value");
    }
};

} //namespace delta

//-----------------------------------------------------------------------------
typedef serialization::binary::Serializer<void>   BinSerializerT;
typedef serialization::binary::Deserializer<void> BinDeserializerT;

typedef serialization::TypeIdMap<BinSerializerT, BinDeserializerT, std::string> TypeIdMapT;

typedef serialization::TypeIdMap<BinSerializerT, BinDeserializerT>         AlphaTypeIdMapT;
typedef serialization::TypeIdMap<BinSerializerT, BinDeserializerT, string> BetaTypeIdMapT;
typedef serialization::TypeIdMap<BinSerializerT, BinDeserializerT>         DeltaTypeIdMapT;

int test_typeidmap(int argc, char* argv[])
{

#ifdef SOLID_HAS_DEBUG
    Debug::the().levelMask("ew");
    Debug::the().moduleMask("all");
    Debug::the().initStdErr(false, nullptr);
#endif

    std::string alpha_data;
    std::string beta_data;
    std::string delta_data;

    { //
        TypeIdMapT typemap;
        //--------------------------------------------------
        typemap.registerType<alpha::Test<1>>(
            "alpha::test<1>",
            static_cast<size_t>(Protocols::Alpha));
        typemap.registerCast<alpha::Test<1>, alpha::Base>();
        typemap.registerType<alpha::Test<2>>(
            "alpha::test<2>",
            static_cast<size_t>(Protocols::Alpha));
        typemap.registerCast<alpha::Test<2>, alpha::Base>();
        //--------------------------------------------------
        typemap.registerType<beta::Test<3>>(
            "beta::test<3>",
            beta::serialize<BinSerializerT, beta::Test<3>>,
            beta::serialize<BinDeserializerT, beta::Test<3>>,
            static_cast<size_t>(Protocols::Beta));
        typemap.registerCast<beta::Test<3>, ::Base>();

        typemap.registerType<beta::Test<4>>(
            "beta::test<4>",
            beta::serialize<BinSerializerT, beta::Test<4>>,
            beta::serialize<BinDeserializerT, beta::Test<4>>,
            static_cast<size_t>(Protocols::Beta));
        typemap.registerCast<beta::Test<4>, ::Base>();
        //--------------------------------------------------
        typemap.registerType<delta::Test<5>>(
            "delta::test<5>",
            static_cast<size_t>(Protocols::Delta));
        typemap.registerCast<delta::Test<5>, delta::Base>();

        typemap.registerType<delta::Test<6>>(
            "delta::test<6>",
            static_cast<size_t>(Protocols::Delta));
        typemap.registerCast<delta::Test<6>, delta::Base>();
        //--------------------------------------------------
        if (1) { //fill alpha data:
            const size_t   bufcp = 64;
            char           buf[bufcp];
            BinSerializerT ser(&typemap);
            int            rv;

            alpha::Test<1> a1("alpha::Test<1>");

            alpha::Test<1>* pa1 = &a1;
            alpha::Base*    pa2 = new alpha::Test<2>("alpha::Test<2>");
            alpha::Base*    pa3 = nullptr;

            ser.push(pa1, "a1").push(pa2, "a2").push(pa3, "a3");

            while ((rv = ser.run(buf, bufcp)) > 0) {
                alpha_data.append(buf, rv);
            }
            if (rv < 0) {
                cout << "ERROR: serialization: " << ser.error().category().name() << ": " << ser.error().message() << endl;
                SOLID_THROW("Serialization error - alpha");
                return 0;
            } else {
                alpha_data.append(buf, rv);
                cout << "alpha data size = " << alpha_data.size() << endl;
            }
            delete pa2;
        }

        { //fill beta data:
            const size_t   bufcp = 64;
            char           buf[bufcp];
            BinSerializerT ser(&typemap);
            int            rv;

            beta::Test<3> a1("beta::Test<3>", 3);

            beta::Base*  pa1 = &a1;
            BasePointerT pa2 = std::make_shared<beta::Test<4>>("beta::Test<4>", 4); //(new beta::Test<4>("beta::Test<4>", 4);

            ser.push(pa1, "a1").push(pa2, "a2");

            while ((rv = ser.run(buf, bufcp)) > 0) {
                beta_data.append(buf, rv);
            }
            if (rv < 0) {
                cout << "ERROR: serialization: " << ser.error().category().name() << ": " << ser.error().message() << endl;
                SOLID_THROW("Serialization error - beta");
                return 0;
            } else {
                beta_data.append(buf, rv);
                cout << "beta data size = " << beta_data.size() << endl;
            }
        }

        if (1) { //fill delta data:
            const size_t   bufcp = 64;
            char           buf[bufcp];
            BinSerializerT ser(&typemap);
            int            rv;

            delta::Test<5> a1("delta::Test<5>");

            delta::Test<5>* pa1 = &a1;
            delta::Base*    pa2 = new delta::Test<6>("delta::Test<6>");

            ser.push(pa1, "a1").push(pa2, "a2");

            while ((rv = ser.run(buf, bufcp)) > 0) {
                delta_data.append(buf, rv);
            }
            if (rv < 0) {
                cout << "ERROR: serialization: " << ser.error().category().name() << ": " << ser.error().message() << endl;
                SOLID_THROW("Serialization error - delta");
                return 0;
            } else {
                delta_data.append(buf, rv);
                cout << "delta data size = " << delta_data.size() << endl;
            }
            delete pa2;
        }
    }

    if (1) { //read alfa
        AlphaTypeIdMapT typemap;
        typemap.registerType<alpha::Test<1>>(
            static_cast<size_t>(Protocols::Alpha));
        typemap.registerCast<alpha::Test<1>, alpha::Base>();
        typemap.registerType<alpha::Test<2>>(
            static_cast<size_t>(Protocols::Alpha));
        typemap.registerCast<alpha::Test<2>, alpha::Base>();

        {
            BinDeserializerT des(&typemap);
            int              rv;

            alpha::Base* pa1 = nullptr;
            alpha::Base* pa2 = nullptr;
            alpha::Base* pa3 = nullptr;

            des.push(pa1, "pa1").push(pa2, "pa2").push(pa3, "pa3");

            rv = des.run(alpha_data.data(), alpha_data.size());

            if (rv != static_cast<int>(alpha_data.size())) {
                cout << "ERROR: deserialization: " << des.error().category().name() << ": " << des.error().message() << endl;
                SOLID_THROW("Deserialization error - alpha");
                return 0;
            }

            if (/*pa1 == nullptr || pa2 == nullptr or*/ pa3 != nullptr) {
                SOLID_THROW("Deserialization error - alpha - empty");
                return 0;
            }

            pa1->print(cout);
            pa2->print(cout);

            if (!pa1->check() || !pa2->check()) {
                SOLID_THROW("Deserialization error - alpha - check");
                return 0;
            }
            delete pa1;
            delete pa2;
        }
    }

    { //read beta
        BetaTypeIdMapT typemap;

        typemap.registerType<beta::Test<3>>(
            "beta::Test<3>",
            beta::serialize<BinSerializerT, beta::Test<3>>,
            beta::serialize<BinDeserializerT, beta::Test<3>>,
            static_cast<size_t>(Protocols::Beta));
        typemap.registerCast<beta::Test<3>, ::Base>();

        typemap.registerType<beta::Test<4>>(
            "beta::Test<4>",
            beta::serialize<BinSerializerT, beta::Test<4>>,
            beta::serialize<BinDeserializerT, beta::Test<4>>,
            static_cast<size_t>(Protocols::Beta));
        typemap.registerCast<beta::Test<4>, beta::Base>();

        {
            BinDeserializerT des(&typemap);
            int              rv;

            BasePointerT       pa1;
            beta::BasePointerT pa2;

            des.push(pa1, "pa1").push(pa2, "pa2");

            rv = des.run(beta_data.data(), beta_data.size());

            if (rv != static_cast<int>(beta_data.size())) {
                cout << "ERROR: deserialization: " << des.error().category().name() << ": " << des.error().message() << endl;
                SOLID_THROW("Deserialization error - beta");
                return 0;
            }

            if (not pa1 || not pa2) {
                SOLID_THROW("Deserialization error - beta - empty");
                return 0;
            }

            pa1->print(cout);
            pa2->print(cout);

            cout << "Data for pa1 = " << typemap[pa1.get()] << endl;
            cout << "Data for pa2 = " << typemap[pa2.get()] << endl;

            if (!pa1->check(&typemap[pa1.get()]) || !pa2->check(&typemap[pa2.get()])) {
                SOLID_THROW("Deserialization error - beta - check");
                return 0;
            }
        }
    }

    if (1) { //read delta
        DeltaTypeIdMapT typemap;
        typemap.registerType<delta::Test<5>>(
            static_cast<size_t>(Protocols::Delta));
        typemap.registerCast<delta::Test<5>, delta::Base>();

        typemap.registerType<delta::Test<6>>(
            static_cast<size_t>(Protocols::Delta));
        typemap.registerCast<delta::Test<6>, delta::Base>();

        {
            BinDeserializerT des(&typemap);
            int              rv;

            delta::Base* pa1 = nullptr;
            delta::Base* pa2 = nullptr;

            des.push(pa1, "pa1").push(pa2, "pa2");

            rv = des.run(delta_data.data(), delta_data.size());

            if (rv != static_cast<int>(delta_data.size())) {
                cout << "ERROR: deserialization: " << des.error().category().name() << ": " << des.error().message() << endl;
                SOLID_THROW("Deserialization error - delta");
                return 0;
            }

            if (pa1 == nullptr || pa2 == nullptr) {
                SOLID_THROW("Deserialization error - delta - empty");
                return 0;
            }

            pa1->print(cout);
            pa2->print(cout);

            if (!pa1->check() || !pa2->check()) {
                SOLID_THROW("Deserialization error - delta - check");
                return 0;
            }
            delete pa1;
            delete pa2;
        }
    }
    return 0;
}
