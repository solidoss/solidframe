#include "solid/frame/manager.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/frame/service.hpp"

#include "solid/frame/aio/aioresolver.hpp"

#include "solid/frame/mprpc/mprpcconfiguration.hpp"
#include "solid/frame/mprpc/mprpcservice.hpp"

#include "mprpc_request_messages.hpp"
#include <deque>
#include <iostream>
#include <regex>
#include <string>

using namespace solid;
using namespace std;

namespace {

using AioSchedulerT = frame::Scheduler<frame::aio::Reactor>;

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
    {"iron_man", "Iron Man", "man.iron@email.com", "UK", "London", {11, 4, 2004}},
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

} //namespace

namespace rpc_request_server {

template <class M>
void complete_message(
    frame::mprpc::ConnectionContext& _rctx,
    std::shared_ptr<M>&              _rsent_msg_ptr,
    std::shared_ptr<M>&              _rrecv_msg_ptr,
    ErrorConditionT const&           _rerror);

template <>
void complete_message<rpc_request::Request>(
    frame::mprpc::ConnectionContext&       _rctx,
    std::shared_ptr<rpc_request::Request>& _rsent_msg_ptr,
    std::shared_ptr<rpc_request::Request>& _rrecv_msg_ptr,
    ErrorConditionT const&                 _rerror)
{
    solid_check(!_rerror);
    solid_check(_rrecv_msg_ptr);
    solid_check(!_rsent_msg_ptr);

    auto msgptr = std::make_shared<rpc_request::Response>(*_rrecv_msg_ptr);

    std::regex userid_regex(_rrecv_msg_ptr->userid_regex);

    for (const auto& ad : account_dq) {
        if (std::regex_match(ad.userid, userid_regex)) {
            msgptr->user_data_map.insert(rpc_request::Response::UserDataMapT::value_type(ad.userid, make_user_data(ad)));
        }
    }

    solid_check(!_rctx.service().sendResponse(_rctx.recipientId(), std::move(msgptr)));
}

template <>
void complete_message<rpc_request::Response>(
    frame::mprpc::ConnectionContext&        _rctx,
    std::shared_ptr<rpc_request::Response>& _rsent_msg_ptr,
    std::shared_ptr<rpc_request::Response>& _rrecv_msg_ptr,
    ErrorConditionT const&                  _rerror)
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
            auto                        proto = frame::mprpc::serialization_v3::create_protocol<reflection::v1::metadata::Variant, uint8_t>(
                reflection::v1::metadata::factory,
                [&](auto &_rmap){
                    auto lambda = [&](const uint8_t _id, const std::string_view _name, auto const &_rtype){
                        using TypeT = typename std::decay_t<decltype(_rtype)>::TypeT;
                        _rmap.template registerMessage<TypeT>(_id, _name, rpc_request_server::complete_message<TypeT>);
                    };
                    rpc_request::configure_protocol(lambda);
                }
            );
            frame::mprpc::Configuration cfg(scheduler, proto);

            cfg.server.listener_address_str = p.listener_addr;
            cfg.server.listener_address_str += ':';
            cfg.server.listener_address_str += p.listener_port;

            cfg.server.connection_start_state = frame::mprpc::ConnectionState::Active;

            rpcservice.start(std::move(cfg));

            {
                std::ostringstream oss;
                oss << rpcservice.configuration().server.listenerPort();
                cout << "server listens on port: " << oss.str() << endl;
            }
        }

        cout << "Press ENTER to stop: ";
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
