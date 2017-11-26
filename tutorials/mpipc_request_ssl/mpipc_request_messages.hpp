
#pragma once

#include "solid/frame/mpipc/mpipccontext.hpp"
#include "solid/frame/mpipc/mpipcmessage.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"
#include <map>
#include <ostream>
#include <vector>

namespace ipc_request {

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

    size_t cache_idx; //NOT serialized - used by the server to cache certain key data
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

    template <class S>
    void solidSerialize(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(second, "second").push(first, "first");
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

    template <class S>
    void solidSerialize(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(second, "second").push(first, "first");
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

    template <class S>
    void solidSerialize(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.pushContainer(key_vec, "key_vec");
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

    template <class S>
    void solidSerialize(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.pushContainer(key_vec, "key_vec");
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

    template <class S>
    void solidSerialize(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(regex, "regex");
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

    template <class S>
    void solidSerialize(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(regex, "regex");
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

    template <class S>
    void solidSerialize(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(year, "year");
    }

    void print(std::ostream& _ros) const override
    {
        _ros << "year < " << year;
    }
};

struct Request : solid::frame::mpipc::Message {
    std::shared_ptr<RequestKey> key;

    Request() {}

    Request(std::shared_ptr<RequestKey>&& _key)
        : key(std::move(_key))
    {
    }

    template <class S>
    void solidSerialize(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(key, "key");
    }
};

struct Date {
    uint8_t  day;
    uint8_t  month;
    uint16_t year;

    template <class S>
    void solidSerialize(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(day, "day").push(month, "month").push(year, "year");
    }
};

struct UserData {
    std::string full_name;
    std::string email;
    std::string country;
    std::string city;
    Date        birth_date;

    template <class S>
    void solidSerialize(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.push(full_name, "full_name").push(email, "email");
        _s.push(country, "country").push(city, "city").push(birth_date, "birth_date");
    }
};

struct Response : solid::frame::mpipc::Message {
    using UserDataMapT = std::map<std::string, UserData>;

    UserDataMapT user_data_map;

    Response() {}

    Response(const solid::frame::mpipc::Message& _rmsg)
        : solid::frame::mpipc::Message(_rmsg)
    {
    }

    template <class S>
    void solidSerialize(S& _s, solid::frame::mpipc::ConnectionContext& _rctx)
    {
        _s.pushContainer(user_data_map, "user_data_map");
    }
};

using ProtoSpecT = solid::frame::mpipc::serialization_v1::ProtoSpec<0,
    Request,
    Response,
    RequestKeyAnd,
    RequestKeyOr,
    RequestKeyAndList,
    RequestKeyOrList,
    RequestKeyUserIdRegex,
    RequestKeyEmailRegex,
    RequestKeyYearLess>;

} //namespace ipc_request
