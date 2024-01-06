// NOTE: on Windows, when compiling against BoringSSL
// we need to include "openssl/ssl.hpp" before any
// inclusion of Windows.h
#include "solid/frame/mprpc/mprpcsocketstub_openssl.hpp"

#include "solid/frame/mprpc/mprpccompression_snappy.hpp"
#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "mprpc_request_messages.hpp"
#include <deque>
#include <iostream>
#include <regex>
#include <string>

using namespace solid;
using namespace std;

namespace {

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor<frame::mprpc::EventT>>;

//-----------------------------------------------------------------------------
//      Parameters
//-----------------------------------------------------------------------------
struct Parameters {
    Parameters()
        : listener_port("0")
        , listener_addr("0.0.0.0")
    {
    }

    string listener_port;
    string listener_addr;
};
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Dummy database
//-----------------------------------------------------------------------------
struct Date {
    uint8_t  day;
    uint8_t  month;
    uint16_t year;

    Date(const uint8_t _day = 0, const uint8_t _month = 0, const uint16_t _year = 0)
        : day(_day)
        , month(_month)
        , year(_year)
    {
    }
};

struct AccountData {
    string userid;
    string full_name;
    string email;
    string country;
    string city;
    Date   birth_date;
};

using AccountDataDequeT = std::deque<AccountData>;

const AccountDataDequeT account_dq = {
    {"user1", "Super Man", "user1@email.com", "US", "Washington", {11, 1, 2001}},
    {"user2", "Spider Man", "user2@email.com", "RO", "Bucharest", {12, 2, 2002}},
    {"user3", "Ant Man", "user3@email.com", "IE", "Dublin", {13, 3, 2003}},
    {"iron_man", "Iron Man", "man.iron@email.com", "UK", "London", {11, 4, 2002}},
    {"dragon_man", "Dragon Man", "man.dragon@email.com", "FR", "paris", {12, 5, 2005}},
    {"frog_man", "Frog Man", "man.frog@email.com", "PL", "Warsaw", {13, 6, 2006}},
};

rpc_request::UserData make_user_data(const AccountData& _rad)
{
    rpc_request::UserData ud;
    ud.full_name        = _rad.full_name;
    ud.email            = _rad.email;
    ud.country          = _rad.country;
    ud.city             = _rad.city;
    ud.birth_date.day   = _rad.birth_date.day;
    ud.birth_date.month = _rad.birth_date.month;
    ud.birth_date.year  = _rad.birth_date.year;
    return ud;
}

} // namespace

//-----------------------------------------------------------------------------
// rpc_request_server - message handling namespace
//-----------------------------------------------------------------------------
namespace rpc_request_server {

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext&  _rctx,
    frame::mprpc::MessagePointerT<M>& _rsent_msg_ptr,
    frame::mprpc::MessagePointerT<M>& _rrecv_msg_ptr,
    ErrorConditionT const&            _rerror);

using namespace rpc_request;

struct PrepareKeyVisitor : RequestKeyVisitor {
    std::vector<std::regex> regexvec;

    void visit(RequestKeyAnd& _k) override
    {
        if (_k.first) {
            _k.first->visit(*this);
        }
        if (_k.second) {
            _k.second->visit(*this);
        }
    }

    void visit(RequestKeyOr& _k) override
    {
        if (_k.first) {
            _k.first->visit(*this);
        }
        if (_k.second) {
            _k.second->visit(*this);
        }
    }

    void visit(RequestKeyAndList& _k) override
    {
        for (auto& k : _k.key_vec) {
            if (k)
                k->visit(*this);
        }
    }

    void visit(RequestKeyOrList& _k) override
    {
        for (auto& k : _k.key_vec) {
            if (k)
                k->visit(*this);
        }
    }

    void visit(RequestKeyUserIdRegex& _k) override
    {
        _k.cache_idx = regexvec.size();
        regexvec.emplace_back(_k.regex);
    }

    void visit(RequestKeyEmailRegex& _k) override
    {
        _k.cache_idx = regexvec.size();
        regexvec.emplace_back(_k.regex);
    }

    void visit(RequestKeyYearLess& /*_k*/) override
    {
    }
};

struct AccountDataKeyVisitor : RequestKeyConstVisitor {
    const AccountData& racc;
    PrepareKeyVisitor& rprep;
    bool               retval;

    AccountDataKeyVisitor(const AccountData& _racc, PrepareKeyVisitor& _rprep)
        : racc(_racc)
        , rprep(_rprep)
        , retval(false)
    {
    }

    void visit(const RequestKeyAnd& _k) override
    {
        retval = false;

        if (_k.first) {
            _k.first->visit(*this);
            if (!retval)
                return;
        } else {
            retval = false;
            return;
        }
        if (_k.second) {
            _k.second->visit(*this);
            if (!retval)
                return;
        }
    }

    void visit(const RequestKeyOr& _k) override
    {
        retval = false;
        if (_k.first) {
            _k.first->visit(*this);
            if (retval)
                return;
        }
        if (_k.second) {
            _k.second->visit(*this);
            if (retval)
                return;
        }
    }

    void visit(const RequestKeyAndList& _k) override
    {
        retval = false;
        for (auto& k : _k.key_vec) {
            if (k) {
                k->visit(*this);
                if (!retval)
                    return;
            }
        }
    }
    void visit(const RequestKeyOrList& _k) override
    {
        for (auto& k : _k.key_vec) {
            if (k) {
                k->visit(*this);
                if (retval)
                    return;
            }
        }
        retval = false;
    }

    void visit(const RequestKeyUserIdRegex& _k) override
    {
        retval = std::regex_match(racc.userid, rprep.regexvec[_k.cache_idx]);
    }
    void visit(const RequestKeyEmailRegex& _k) override
    {
        retval = std::regex_match(racc.email, rprep.regexvec[_k.cache_idx]);
    }

    void visit(const RequestKeyYearLess& _k) override
    {
        retval = racc.birth_date.year < _k.year;
    }
};

template <>
void complete_message<rpc_request::Request>(
    frame::mprpc::ConnectionContext&                     _rctx,
    frame::mprpc::MessagePointerT<rpc_request::Request>& _rsent_msg_ptr,
    frame::mprpc::MessagePointerT<rpc_request::Request>& _rrecv_msg_ptr,
    ErrorConditionT const&                               _rerror)
{
    solid_check(!_rerror);
    solid_check(_rrecv_msg_ptr);
    solid_check(!_rsent_msg_ptr);

    cout << "Received request: ";
    if (_rrecv_msg_ptr->key) {
        _rrecv_msg_ptr->key->print(cout);
    }
    cout << endl;

    auto msgptr = frame::mprpc::make_message<rpc_request::Response>(*_rrecv_msg_ptr);

    if (_rrecv_msg_ptr->key) {
        PrepareKeyVisitor prep;

        _rrecv_msg_ptr->key->visit(prep);

        for (const auto& ad : account_dq) {
            AccountDataKeyVisitor v(ad, prep);

            _rrecv_msg_ptr->key->visit(v);

            if (v.retval) {
                msgptr->user_data_map.insert(rpc_request::Response::UserDataMapT::value_type(ad.userid, make_user_data(ad)));
            }
        }
    }

    solid_check(!_rctx.service().sendResponse(_rctx.recipientId(), std::move(msgptr)));
}

template <>
void complete_message<rpc_request::Response>(
    frame::mprpc::ConnectionContext&                      _rctx,
    frame::mprpc::MessagePointerT<rpc_request::Response>& _rsent_msg_ptr,
    frame::mprpc::MessagePointerT<rpc_request::Response>& _rrecv_msg_ptr,
    ErrorConditionT const&                                _rerror)
{
    solid_check(!_rerror);
    solid_check(!_rrecv_msg_ptr);
    solid_check(_rsent_msg_ptr);
}

} // namespace rpc_request_server

//-----------------------------------------------------------------------------

bool parseArguments(Parameters& _par, int argc, char* argv[]);

//-----------------------------------------------------------------------------
//      main
//-----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    Parameters p;

    if (!parseArguments(p, argc, argv))
        return 0;

    {

        AioSchedulerT          scheduler;
        frame::Manager         manager;
        frame::mprpc::ServiceT rpcservice(manager);
        ErrorConditionT        err;

        scheduler.start(1);

        {
            auto proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto& _rmap) {
                    auto lambda = [&](const uint8_t _id, const std::string_view _name, auto const& _rtype) {
                        using TypeT = typename std::decay_t<decltype(_rtype)>::TypeT;

                        if constexpr (std::is_base_of_v<rpc_request::RequestKey, TypeT>) {
                            _rmap.template registerType<TypeT, rpc_request::RequestKey>(_id, _name);
                        } else {
                            _rmap.template registerMessage<TypeT>(_id, _name, rpc_request_server::complete_message<TypeT>);
                        }
                    };
                    rpc_request::configure_protocol(lambda);
                });
            frame::mprpc::Configuration cfg(scheduler, proto);

            cfg.server.listener_address_str = p.listener_addr;
            cfg.server.listener_address_str += ':';
            cfg.server.listener_address_str += p.listener_port;

            cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;

            frame::mprpc::openssl::setup_server(
                cfg,
                [](frame::aio::openssl::Context& _rctx) -> ErrorCodeT {
                    _rctx.loadVerifyFile("echo-ca-cert.pem");
                    _rctx.loadCertificateFile("echo-server-cert.pem");
                    _rctx.loadPrivateKeyFile("echo-server-key.pem");
                    return ErrorCodeT();
                },
                frame::mprpc::openssl::NameCheckSecureStart{"echo-client"} // does nothing - OpenSSL does not check for hostname on SSL_accept
            );

            frame::mprpc::snappy::setup(cfg);

            {
                frame::mprpc::ServiceStartStatus start_status;
                rpcservice.start(start_status, std::move(cfg));

                cout << "server listens on: " << start_status.listen_addr_vec_.back() << endl;
            }
        }

        cout << "Press ENTER to stop:" << endl;
        cin.ignore();
    }
    return 0;
}

//-----------------------------------------------------------------------------

bool parseArguments(Parameters& _par, int argc, char* argv[])
{
    if (argc == 2) {
        size_t pos;

        _par.listener_addr = argv[1];

        pos = _par.listener_addr.rfind(':');

        if (pos != string::npos) {
            _par.listener_addr[pos] = '\0';

            _par.listener_port.assign(_par.listener_addr.c_str() + pos + 1);

            _par.listener_addr.resize(pos);
        }
    }
    return true;
}
