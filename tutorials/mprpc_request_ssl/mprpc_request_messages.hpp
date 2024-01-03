
#pragma once

#include "solid/frame/mprpc/mprpccontext.hpp"
#include "solid/frame/mprpc/mprpcmessage.hpp"
#include "solid/frame/mprpc/mprpcprotocol_serialization_v3.hpp"
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

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.first, _rctx, 1, "first").add(_rthis.second, _rctx, 2, "second");
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

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.first, _rctx, 1, "first").add(_rthis.second, _rctx, 2, "second");
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

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.key_vec, _rctx, 1, "key_vec");
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

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.key_vec, _rctx, 1, "key_vec");
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

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.regex, _rctx, 1, "regex");
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

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.regex, _rctx, 1, "regex");
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

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.year, _rctx, 1, "year");
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

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.key, _rctx, 1, "key");
    }
};

struct Date {
    uint8_t  day;
    uint8_t  month;
    uint16_t year;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.day, _rctx, 1, "day").add(_rthis.month, _rctx, 2, "month").add(_rthis.year, _rctx, 3, "year");
    }
};

struct UserData {
    std::string full_name;
    std::string email;
    std::string country;
    std::string city;
    Date        birth_date;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.full_name, _rctx, 1, "full_name").add(_rthis.email, _rctx, 2, "email").add(_rthis.country, _rctx, 3, "country");
        _rr.add(_rthis.city, _rctx, 4, "city").add(_rthis.birth_date, _rctx, 5, "birth_date");
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

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.user_data_map, _rctx, 1, "user_data_map");
    }
};

template <class Reg>
inline void configure_protocol(Reg _rreg)
{
    _rreg(1, "Request", solid::TypeToType<Request>());
    _rreg(2, "Response", solid::TypeToType<Response>());
    _rreg(3, "RequestKeyAnd", solid::TypeToType<RequestKeyAnd>());
    _rreg(4, "RequestKeyOr", solid::TypeToType<RequestKeyOr>());
    _rreg(5, "RequestKeyAndList", solid::TypeToType<RequestKeyAndList>());
    _rreg(6, "RequestKeyOrList", solid::TypeToType<RequestKeyOrList>());
    _rreg(7, "RequestKeyUserIdRegex", solid::TypeToType<RequestKeyUserIdRegex>());
    _rreg(8, "RequestKeyEmailRegex", solid::TypeToType<RequestKeyEmailRegex>());
    _rreg(9, "RequestKeyYearLess", solid::TypeToType<RequestKeyYearLess>());
}

} // namespace rpc_request
