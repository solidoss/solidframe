
#pragma once

#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v4.hpp"
#include <map>
#include <ostream>
#include <vector>

namespace rpc_request {

struct RequestKeyVisitor;
struct RequestKeyConstVisitor;

struct RequestKey {
    RequestKey()
        : cache_idx(solid::InvalidIndex{})
    {
    }
    virtual ~RequestKey() {}

    virtual void print(std::ostream& _ros) const = 0;

    virtual void visit(RequestKeyVisitor&)            = 0;
    virtual void visit(RequestKeyConstVisitor&) const = 0;

    size_t cache_idx; // NOT serialized - used by the server to cache certain key data
};

struct RequestKeyAnd;
struct RequestKeyOr;
struct RequestKeyAndList;
struct RequestKeyOrList;
struct RequestKeyUserIdRegex;
struct RequestKeyEmailRegex;
struct RequestKeyYearLess;

struct RequestKeyVisitor {
    virtual ~RequestKeyVisitor() {}

    virtual void visit(RequestKeyAnd&)         = 0;
    virtual void visit(RequestKeyOr&)          = 0;
    virtual void visit(RequestKeyAndList&)     = 0;
    virtual void visit(RequestKeyOrList&)      = 0;
    virtual void visit(RequestKeyUserIdRegex&) = 0;
    virtual void visit(RequestKeyEmailRegex&)  = 0;
    virtual void visit(RequestKeyYearLess&)    = 0;
};

struct RequestKeyConstVisitor {
    virtual ~RequestKeyConstVisitor() {}

    virtual void visit(const RequestKeyAnd&)         = 0;
    virtual void visit(const RequestKeyOr&)          = 0;
    virtual void visit(const RequestKeyAndList&)     = 0;
    virtual void visit(const RequestKeyOrList&)      = 0;
    virtual void visit(const RequestKeyUserIdRegex&) = 0;
    virtual void visit(const RequestKeyEmailRegex&)  = 0;
    virtual void visit(const RequestKeyYearLess&)    = 0;
};

template <class T>
struct Visitable : RequestKey {

    void visit(RequestKeyVisitor& _v) override
    {
        _v.visit(*static_cast<T*>(this));
    }
    void visit(RequestKeyConstVisitor& _v) const override
    {
        _v.visit(*static_cast<const T*>(this));
    }
};

struct RequestKeyAnd : Visitable<RequestKeyAnd> {
    std::shared_ptr<RequestKey> first;
    std::shared_ptr<RequestKey> second;

    RequestKeyAnd() {}

    template <class T1, class T2>
    RequestKeyAnd(
        std::shared_ptr<T1>&& _p1,
        std::shared_ptr<T2>&& _p2)
        : first(std::move(std::static_pointer_cast<RequestKey>(_p1)))
        , second(std::move(std::static_pointer_cast<RequestKey>(_p2)))
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("first", _rthis.first), _rctx);
        _rr(id<2>, make("second", _rthis.second), _rctx);
    }

    void print(std::ostream& _ros) const override
    {
        _ros << "and{";
        if (first)
            first->print(_ros);
        _ros << ',';
        if (second)
            second->print(_ros);
        _ros << '}';
    }
};

struct RequestKeyOr : Visitable<RequestKeyOr> {
    std::shared_ptr<RequestKey> first;
    std::shared_ptr<RequestKey> second;

    RequestKeyOr() {}

    template <class T1, class T2>
    RequestKeyOr(
        std::shared_ptr<T1>&& _p1,
        std::shared_ptr<T2>&& _p2)
        : first(std::move(std::static_pointer_cast<RequestKey>(_p1)))
        , second(std::move(std::static_pointer_cast<RequestKey>(_p2)))
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("first", _rthis.first), _rctx);
        _rr(id<2>, make("second", _rthis.second), _rctx);
    }

    void print(std::ostream& _ros) const override
    {
        _ros << "or(";
        if (first)
            first->print(_ros);
        _ros << ',';
        if (second)
            second->print(_ros);
        _ros << ')';
    }
};

struct RequestKeyAndList : Visitable<RequestKeyAndList> {
    std::vector<std::shared_ptr<RequestKey>> key_vec;

    RequestKeyAndList() {}

    template <class... Args>
    RequestKeyAndList(std::shared_ptr<Args>&&... _args)
        : key_vec{std::move(_args)...}
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("key_vec", _rthis.key_vec), _rctx);
    }

    void print(std::ostream& _ros) const override
    {
        _ros << "AND{";
        for (auto const& key : key_vec) {
            if (key)
                key->print(_ros);
            _ros << ',';
        }
        _ros << '}';
    }
};

struct RequestKeyOrList : Visitable<RequestKeyOrList> {
    std::vector<std::shared_ptr<RequestKey>> key_vec;

    RequestKeyOrList() {}

    template <class... Args>
    RequestKeyOrList(std::shared_ptr<Args>&&... _args)
        : key_vec{std::move(_args)...}
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("key_vec", _rthis.key_vec), _rctx);
    }

    void print(std::ostream& _ros) const override
    {
        _ros << "OR(";
        for (auto const& key : key_vec) {
            if (key)
                key->print(_ros);
            _ros << ',';
        }
        _ros << ')';
    }
};

struct RequestKeyUserIdRegex : Visitable<RequestKeyUserIdRegex> {
    std::string regex;

    RequestKeyUserIdRegex() {}

    RequestKeyUserIdRegex(std::string&& _ustr)
        : regex(std::move(_ustr))
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("regex", _rthis.regex), _rctx);
    }

    void print(std::ostream& _ros) const override
    {
        _ros << "userid matches \"" << regex << "\"";
    }
};

struct RequestKeyEmailRegex : Visitable<RequestKeyEmailRegex> {
    std::string regex;

    RequestKeyEmailRegex() {}

    RequestKeyEmailRegex(std::string&& _ustr)
        : regex(std::move(_ustr))
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("regex", _rthis.regex), _rctx);
    }

    void print(std::ostream& _ros) const override
    {
        _ros << "email matches \"" << regex << "\"";
    }
};

struct RequestKeyYearLess : Visitable<RequestKeyYearLess> {
    uint16_t year;

    RequestKeyYearLess(uint16_t _year = 0xffff)
        : year(_year)
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("year", _rthis.year), _rctx);
    }

    void print(std::ostream& _ros) const override
    {
        _ros << "year < " << year;
    }
};

struct Request : solid::frame::mprpc::Message {
    std::shared_ptr<RequestKey> key;

    Request() {}

    Request(std::shared_ptr<RequestKey>&& _key)
        : key(std::move(_key))
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("key", _rthis.key), _rctx);
    }
};

struct Date {
    uint8_t  day;
    uint8_t  month;
    uint16_t year;

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("day", _rthis.day), _rctx);
        _rr(id<2>, make("month", _rthis.month), _rctx);
        _rr(id<3>, make("year", _rthis.year), _rctx);
    }
};

struct UserData {
    std::string full_name;
    std::string email;
    std::string country;
    std::string city;
    Date        birth_date;

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("full_name", _rthis.full_name), _rctx);
        _rr(id<2>, make("email", _rthis.email), _rctx);
        _rr(id<3>, make("country", _rthis.country), _rctx);
        _rr(id<4>, make("city", _rthis.city), _rctx);
        _rr(id<5>, make("birth_date", _rthis.birth_date), _rctx);
    }
};

struct Response : solid::frame::mprpc::Message {
    using UserDataMapT = std::map<std::string, UserData>;

    UserDataMapT user_data_map;

    Response() {}

    Response(const solid::frame::mprpc::Message& _rmsg)
        : solid::frame::mprpc::Message(_rmsg)
    {
    }

    template <typename Reflector, typename Context>
    void solidReflectV2(this auto& _rthis, Reflector&& _rr, Context& _rctx)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("user_data_map", _rthis.user_data_map), _rctx);
    }
};

template <class Reg>
inline void configure_protocol(Reg _rreg)
{
    _rreg(1, "Request", std::type_identity<Request>());
    _rreg(2, "Response", std::type_identity<Response>());
    _rreg(3, "RequestKeyAnd", std::type_identity<RequestKeyAnd>());
    _rreg(4, "RequestKeyOr", std::type_identity<RequestKeyOr>());
    _rreg(5, "RequestKeyAndList", std::type_identity<RequestKeyAndList>());
    _rreg(6, "RequestKeyOrList", std::type_identity<RequestKeyOrList>());
    _rreg(7, "RequestKeyUserIdRegex", std::type_identity<RequestKeyUserIdRegex>());
    _rreg(8, "RequestKeyEmailRegex", std::type_identity<RequestKeyEmailRegex>());
    _rreg(9, "RequestKeyYearLess", std::type_identity<RequestKeyYearLess>());
}

} // namespace rpc_request
