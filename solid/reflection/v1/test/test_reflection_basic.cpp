#include "solid/reflection/reflection.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/typetraits.hpp"
#include <fstream>
#include <future>
#include <iostream>
#include <limits>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

using namespace std;
using namespace solid;
namespace {

enum FigureE {
    Zero,
    One,
    Two,
    Three,
    Four,
    Five,
    Six,
    Seven,
    Eight,
    Nine
};

enum UserStatus {
    StatusMarriedE,
    StatusSingleE,
    StatusRelationE,
};

//const EnumMap figure_enum_map = {{Zero, "zero"}, {One, "one"}};
const reflection::EnumMap figure_enum_map{TypeToType<FigureE>(), {{Zero, "Zero"}, {One, "One"}, {Two, "Two"}, {Three, "Three"}, {Four, "Four"}, {Five, "Five"}, {Six, "Six"}, {Seven, "Seven"}, {Eight, "Eight"}, {Nine, "Nine"}}};

const reflection::EnumMap user_status_map = {TypeToType<UserStatus>(), {{StatusMarriedE, "Married"}, {StatusSingleE, "Single"}, {StatusRelationE, "Relation"}}};

struct Fruit {
    virtual ~Fruit() {}
    virtual void print(ostream& _ros) = 0;
};

struct Orange : Fruit {
    void print(ostream& _ros) override
    {
        _ros << "Orange";
    }
    string name_ = "Orange";
    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.name_, _rctx, 1, "name");
    }
};

struct Apple : Fruit {
    void print(ostream& _ros) override
    {
        _ros << "Apple";
    }
    string name_ = "Apple";
    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.name_, _rctx, 1, "name");
    }
};

struct Kiwi : Fruit {
    void print(ostream& _ros) override
    {
        _ros << "Kiwi";
    }
    string name_ = "Kiwi";
    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.name_, _rctx, 1, "name");
    }
};

struct Veg {
    virtual ~Veg() {}
    virtual void print(ostream& _ros) = 0;
};

struct Cucumber : Veg {
    void print(ostream& _ros) override
    {
        _ros << "Cucumber";
    }
    string name_ = "Cucumber";
    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.name_, _rctx, 1, "name");
    }
};

struct Potato : Veg {
    void print(ostream& _ros) override
    {
        _ros << "Potato";
    }
    string name_ = "Potato";
    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.name_, _rctx, 1, "name");
    }
};

struct Carrot : Veg {
    void print(ostream& _ros) override
    {
        _ros << "Carrot";
    }
    string name_ = "Carrot";
    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.name_, _rctx, 1, "name");
    }
};

using VegTupleT = std::tuple<Cucumber, Potato, Carrot>;

const reflection::TypeMapBase* pfruits_map = nullptr;

struct Test {
    static constexpr size_t max_service_count = 4;
    struct User {
        string         name_;
        string         id_;
        string         email_;
        uint16_t       weight_;
        uint16_t       height_;
        UserStatus     status_;
        vector<string> service_vec_;

        SOLID_REFLECT_V1(_rr, _rthis, _rctx)
        {
            _rr.add(_rthis.name_, _rctx, 1, "name", [](auto& _rmeta) { _rmeta.maxSize(20).minSize(8); });
            _rr.add(_rthis.id_, _rctx, 2, "id", [](auto& _rmeta) { _rmeta.minSize(10).maxSize(10); });
            _rr.add(_rthis.email_, _rctx, 3, "email", [](auto& _rmeta) { _rmeta.minSize(5).maxSize(100); });
            _rr.add(_rthis.weight_, _rctx, 4, "weight", [](auto& _rmeta) { _rmeta.min(10).max(1000); });
            _rr.add(_rthis.height_, _rctx, 5, "height", [](auto& _rmeta) { _rmeta.min(10).max(500); });
            _rr.add(_rthis.service_vec_, _rctx, 6, "service_vec", [](auto& _rmeta) { _rmeta.minSize(0).maxSize(max_service_count); });
            _rr.add(_rthis.status_, _rctx, 7, "status");
        }
    };
    vector<User> user_vec_ = {
        {
            "gigi",
            "1",
            "gigi@gigi.com",
            80,
            180,
            StatusMarriedE,
            {"mimi", "bibi", "cici"},
        },
        {
            "vivi",
            "2",
            "vivi@vivi.com",
            60,
            160,
            StatusSingleE,
            {"fifi", "lili", "sisi"},
        },
        {
            "zizi",
            "3",
            "zizi@zizi.com",
            70,
            170,
            StatusRelationE,
            {"pipi", "titi", "qiqi"},
        }};
    struct Service {
        uint64_t       id_;
        string         name_;
        vector<string> ip_vec_;
        FigureE        figure_;

        SOLID_REFLECT_V1(_rr, _rthis, _rctx)
        {
            _rr.add([&_rthis](Reflector& _rr, Context& _rctx) {
                _rr.add(_rthis.id_, _rctx, 1, "id");
                _rr.add(_rthis.name_, _rctx, 2, "name");
                _rr.add(_rthis.ip_vec_, _rctx, 3, "ip_vec");
                _rr.add(_rthis.figure_, _rctx, 4, "figure", [](auto& _rmeta) { _rmeta.map(figure_enum_map); });
            },
                _rctx);
        }
    } services_[max_service_count] = {
        {1, "one", {"122.122.122.111", "122.122.122.112"}, FigureE::One},
        {2, "two", {"122.122.122.211", "122.122.122.212"}, FigureE::Two},
        {3, "three", {"122.122.122.311", "122.122.122.312"}, FigureE::Three},
        {4, "four", {"122.122.122.411", "122.122.122.412"}, FigureE::Four}

    };

    bitset<10>         bitmask_{"1010101010"};
    shared_ptr<Veg>    vegetable_ptr_; // = make_shared<Cucumber>();
    unique_ptr<Fruit>  fruit_ptr_ = make_unique<Apple>();
    shared_ptr<string> guid_ptr_  = make_shared<string>("fasdfasdfweqrweqrwedsfa");
    ifstream           ifs_;
    ofstream           ofs_;
    VegTupleT          veg_tuple_;

public:
    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        using ReflectorT = std::decay_t<decltype(_rr)>;

        _rr.add(_rthis.user_vec_, _rctx, 1, "user_vec");
        _rr.add(_rthis.services_, _rctx, 2, "services");
        _rr.add(_rthis.vegetable_ptr_, _rctx, 4, "vegetable");
        _rr.add(_rthis.fruit_ptr_, _rctx, 5, "fruit", [](auto& _rmeta) { _rmeta.map(*pfruits_map); });
        _rr.add(_rthis.guid_ptr_, _rctx, 6, "guid");
        _rr.add(_rthis.veg_tuple_, _rctx, 7, "veg_tuple");
        if constexpr (!ReflectorT::is_const_reflector) {
            auto progress_lambda = [](Context& _rctx, std::ostream& _ris, uint64_t _len, const bool _done, const size_t _index, const char* _name) {
                //NOTE: here you can use context.any()for actual implementation
            };
            _rr.add(_rthis.ofs_, _rctx, 8, "stream", [&progress_lambda](auto _rmeta) { _rmeta.progressFunction(progress_lambda); });
        } else {
            auto progress_lambda = [](Context& _rctx, std::istream& _ris, uint64_t _len, const bool _done, const size_t _index, const char* _name) {
                //NOTE: here you can use context.any()for actual implementation
            };
            _rr.add(_rthis.ifs_, _rctx, 8, "stream", [&progress_lambda](auto _rmeta) { _rmeta.progressFunction(progress_lambda); });
        }
    }
};

struct OStreamVisitor {
    bool          do_indent_ = true;
    size_t        indent_    = 0;
    std::ostream& rostream_;
    OStreamVisitor(std::ostream& _rostream)
        : rostream_(_rostream)
    {
    }

    void indent()
    {
        for (size_t i = 0; i < indent_; ++i) {
            rostream_ << "    ";
        }
    }

    template <class Node, class Meta, class Ctx>
    void operator()(Node& _rnode, const Meta& _rmeta, const size_t _index, const char* const _name, Ctx& _rctx)
    {
        using tg = reflection::TypeGroupE;
        if (do_indent_) {
            indent();
        } else {
            do_indent_ = true;
        }
        switch (_rnode.typeGroup()) {
        case tg::Basic: {
            auto& rbasic_node = *_rnode.template as<tg::Basic>();
            rostream_ << _name << '(' << _index << ") = ";
            rbasic_node.print(rostream_);
            rostream_ << endl;
        } break;
        case tg::Bitset: {
            auto& rbasic_node = *_rnode.template as<tg::Bitset>();
            rostream_ << _name << '(' << _index << ") = ";
            rbasic_node.print(rostream_);
            rostream_ << endl;
        } break;
        case tg::Enum: {
            auto& renum_node = *_rnode.template as<tg::Enum>();
            rostream_ << _name << '(' << _index << ") = ";
            renum_node.print(rostream_, _rmeta.enumeration()->map());
            rostream_ << endl;
        } break;
        case tg::Structure:
            rostream_ << _name << '(' << _index << ") = {" << endl;
            ++indent_;
            _rnode.template as<tg::Structure>()->for_each(std::ref(*this), _rctx);
            --indent_;
            indent();
            rostream_ << "}" << endl;
            break;
        case tg::Container:
            rostream_ << _name << '(' << _index << ") = [" << endl;
            ++indent_;
            _rnode.template as<tg::Container>()->for_each(std::ref(*this), _rctx);
            --indent_;
            indent();
            rostream_ << "]" << endl;
            break;
        case tg::Array:
            rostream_ << _name << '(' << _index << ") = [" << endl;
            ++indent_;
            _rnode.template as<tg::Array>()->for_each(std::ref(*this), _rctx);
            --indent_;
            indent();
            rostream_ << "]" << endl;
            break;
        case tg::IStream:
            rostream_ << _name << '(' << _index << ") = <istream>" << endl;
            break;
        case tg::OStream:
            rostream_ << _name << '(' << _index << ") = <ostream>" << endl;
            break;
        case tg::Tuple:
            rostream_ << _name << '(' << _index << ") = {" << endl;
            ++indent_;
            _rnode.template as<tg::Tuple>()->for_each(std::ref(*this), _rctx);
            --indent_;
            indent();
            rostream_ << "}" << endl;
            break;
        case tg::SharedPtr: {
            rostream_ << _name << '(' << _index << ") -> ";
            auto& rptr_node = *_rnode.template as<tg::SharedPtr>();
            if (rptr_node.isNull()) {
                if constexpr (!std::is_const_v<Node>) {
                    rptr_node.reset("Carrot", _rmeta.pointer()->map(), _rctx);
                    solid_check(!rptr_node.isNull());
                }
            }
            if (!rptr_node.isNull()) {
                do_indent_ = false;
                rptr_node.visit(std::ref(*this), _rmeta.pointer()->map(), _rctx);
            } else {
                rostream_ << "nullptr" << endl;
            }
            //rostream_ << endl;
        } break;
        case tg::UniquePtr: {
            rostream_ << _name << '(' << _index << ") -> ";
            auto& rptr_node = *_rnode.template as<tg::UniquePtr>();

            if (!rptr_node.isNull()) {
                do_indent_ = false;
                rptr_node.visit(std::ref(*this), _rmeta.pointer()->map(), _rctx);
            } else {
                rostream_ << "nullptr" << endl;
            }
            //rostream_ << endl;
        } break;
        default:
            break;
        }
    }
};

} //namespace

int test_reflection_basic(int argc, char* argv[])
{
    const uint32_t wait_seconds = 5;
    //static_assert(reflection::is_reflective<Test>::value, "Test must be reflective");
    //static_assert(!reflection::is_reflective<std::string>::value, "std::string must not be reflective");
    using ContextT        = solid::EmptyType;
    using ReflectorT      = reflection::ReflectorT<reflection::metadata::Variant<ContextT>, std::decay_t<decltype(reflection::metadata::factory)>, ContextT>;
    using ConstReflectorT = reflection::ConstReflectorT<reflection::metadata::Variant<ContextT>, std::decay_t<decltype(reflection::metadata::factory)>, ContextT>;

    const reflection::TypeMap<ReflectorT, ConstReflectorT> fruit_type_map{
        [](auto& _rmap) {
            _rmap.template registerType<Orange, Fruit>(0, 1, "Orange");
            _rmap.template registerType<Apple, Fruit>(0, 2, "Apple");
            _rmap.template registerType<Kiwi>(0, 3, "Kiwi");
            _rmap.template registerCast<Kiwi, Fruit>();
        }};
    const reflection::TypeMap<ConstReflectorT, ReflectorT> all_type_map{
        [](auto& _rmap) {
            _rmap.template registerType<Cucumber, Veg>(0, 1, "Cucumber");
            _rmap.template registerType<Potato, Veg>(0, 2, "Potato");
            _rmap.template registerType<Carrot, Veg>(0, 3, "Carrot");
            _rmap.template registerType<Orange, Fruit>(0, 4, "Orange");
            _rmap.template registerType<Apple, Fruit>(0, 5, "Apple");
            _rmap.template registerType<Kiwi, Fruit>(0, 6, "Kiwi");
        }};

    pfruits_map = &fruit_type_map;
    auto lambda = [&]() {
        {
            Test             test;
            ostringstream    oss;
            OStreamVisitor   visitor{oss};
            solid::EmptyType context;

            reflection::reflect<reflection::metadata::Variant<ContextT>>(reflection::metadata::factory, test, std::ref(visitor), context, &all_type_map);
            const string result = oss.str();
            cout << result << endl;
            solid_check(result.find("email(3) = gigi@gigi.com") != string::npos);
            solid_check(result.find("veg_tuple(7) = {") != string::npos);
            solid_check(result.find("guid(6) -> (0) = fasdfasdfweqrweqrwedsfa") != string::npos);
            solid_check(result.find("stream(8) = <ostream>") != string::npos);
            solid_check(result.find("vegetable(4) -> (0) = {") != string::npos);
            solid_check(result.find("fruit(5) -> (0) = {") != string::npos);
            solid_check(result.find("name(1) = Carrot") != string::npos);
            solid_check(result.find("name(1) = Apple") != string::npos);
        }

        {
            Test             test;
            ostringstream    oss;
            OStreamVisitor   visitor{oss};
            solid::EmptyType context;

            reflection::const_reflect<reflection::metadata::Variant<ContextT>>(reflection::metadata::factory, test, std::ref(visitor), context, &all_type_map);
            const string result = oss.str();
            cout << result << endl;

            solid_check(result.find("email(3) = gigi@gigi.com") != string::npos);
            solid_check(result.find("veg_tuple(7) = {") != string::npos);
            solid_check(result.find("guid(6) -> (0) = fasdfasdfweqrweqrwedsfa") != string::npos);
            solid_check(result.find("stream(8) = <istream>") != string::npos);
            solid_check(result.find("vegetable(4) -> nullptr") != string::npos);
            solid_check(result.find("fruit(5) -> (0) = {") != string::npos);
            solid_check(result.find("name(1) = Carrot") != string::npos);
            solid_check(result.find("name(1) = Apple") != string::npos);
        }
    };

    auto fut = async(launch::async, lambda);
    if (fut.wait_for(chrono::seconds(wait_seconds)) != future_status::ready) {

        solid_throw(" Test is taking too long - waited " << wait_seconds << " secs");
    }
    fut.get();
    return 0;
}
