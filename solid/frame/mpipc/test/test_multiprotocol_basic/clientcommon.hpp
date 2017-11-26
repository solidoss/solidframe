
#pragma once

#include "solid/system/common.hpp"

#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/scheduler.hpp"

#include "solid/frame/aio/aioresolver.hpp"
#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"
#include "solid/frame/mpipc/mpipcservice.hpp"
#include "solid/system/error.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>

typedef solid::frame::Scheduler<solid::frame::aio::Reactor> AioSchedulerT;

struct Context {
    AioSchedulerT&               rsched;
    solid::frame::Manager&       rm;
    solid::frame::aio::Resolver& rresolver;
    size_t                       max_per_pool_connection_count;
    const std::string&           rserver_port;
    std::atomic<size_t>&         rwait_count;
    std::mutex&                  rmtx;
    std::condition_variable&     rcnd;

    Context(
        AioSchedulerT& _rsched, solid::frame::Manager& _rm, solid::frame::aio::Resolver& _rresolver,
        size_t _max_per_pool_connection_count, const std::string& _rserver_port,
        std::atomic<size_t>&     _rwait_count,
        std::mutex&              _rmtx,
        std::condition_variable& _rcnd)
        : rsched(_rsched)
        , rm(_rm)
        , rresolver(_rresolver)
        , max_per_pool_connection_count(_max_per_pool_connection_count)
        , rserver_port(_rserver_port)
        , rwait_count(_rwait_count)
        , rmtx(_rmtx)
        , rcnd(_rcnd)
    {
    }
};
