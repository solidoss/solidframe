#ifndef TEST_MULTIPROTOCOL_CLIENT_COMMON_HPP
#define TEST_MULTIPROTOCOL_CLIENT_COMMON_HPP

#include "system/common.hpp"

#include "frame/scheduler.hpp"
#include "frame/aio/aioreactor.hpp"

#include "frame/ipc/ipcprotocol_serialization_v1.hpp"
#include "frame/ipc/ipcservice.hpp"
#include "frame/aio/aioresolver.hpp"
#include "system/error.hpp"
#include <memory>
#include <mutex>
#include <condition_variable>


typedef solid::frame::Scheduler<solid::frame::aio::Reactor>	AioSchedulerT;

struct Context{
	AioSchedulerT 				&rsched;
	solid::frame::Manager		&rm;
	solid::frame::aio::Resolver &rresolver;
	size_t 						max_per_pool_connection_count;
	const std::string 			&rserver_port;
	std::atomic<size_t>			&rwait_count;
	std::mutex					&rmtx;
	std::condition_variable		&rcnd;
	
	Context(
		AioSchedulerT &_rsched, solid::frame::Manager &_rm, solid::frame::aio::Resolver &_rresolver,
		size_t _max_per_pool_connection_count, const std::string &_rserver_port,
		std::atomic<size_t> &_rwait_count,
		std::mutex &_rmtx,
		std::condition_variable &_rcnd
	):rsched(_rsched), rm(_rm), rresolver(_rresolver), max_per_pool_connection_count(_max_per_pool_connection_count),
	rserver_port(_rserver_port), rwait_count(_rwait_count), rmtx(_rmtx), rcnd(_rcnd) {}
};

#endif