#pragma once

#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/scheduler.hpp"
#include "solid/system/common.hpp"

#include "solid/frame/mprpc/mprpcprotocol_serialization_v3.hpp"

typedef solid::frame::Scheduler<solid::frame::aio::Reactor> AioSchedulerT;

using TypeIdT = std::pair<uint8_t, uint8_t>;
