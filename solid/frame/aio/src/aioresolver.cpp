// solid/frame/aio/src/aioresolver.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/frame/aio/aioresolver.hpp"
#include "solid/utility/dynamicpointer.hpp"
#include "solid/utility/workpool.hpp"

#include <memory>

namespace solid {
namespace frame {
namespace aio {

typedef DynamicPointer<ResolveBase> ResolverPointerT;

struct WorkPoolController;
typedef WorkPool<ResolverPointerT, WorkPoolController> WorkPoolT;

struct WorkPoolController : solid::WorkPoolController{
    WorkPoolController(const size_t _max_thr_cnt, const size_t _max_job_cnt)
        : solid::WorkPoolController(_max_thr_cnt, _max_job_cnt)
    {
    }

    void execute(WorkPoolBase& _wp, WorkerBase&, ResolverPointerT& _ptr)
    {
        _ptr->run(_wp.isStopping());
    }
};

struct Resolver::Data {
    Data(const size_t _max_thr_cnt, const size_t _max_job_cnt)
        : wp(_max_thr_cnt, _max_job_cnt)
    {
    }

    WorkPoolT wp;
    size_t    thrcnt;
};

/*virtual*/ ResolveBase::~ResolveBase()
{
}

Resolver::Resolver(const size_t _max_thr_cnt, const size_t _max_job_cnt)
    : impl_(make_pimpl<Data>(_max_thr_cnt, _max_job_cnt))
{
}

Resolver::~Resolver()
{
}

ErrorConditionT Resolver::start(ushort _thrcnt)
{
    impl_->wp.start(_thrcnt);
    //TODO: compute a proper error response
    return ErrorConditionT();
}

void Resolver::stop()
{
    impl_->wp.stop();
}

void Resolver::doSchedule(ResolveBase* _pb)
{
    ResolverPointerT ptr(_pb);
    impl_->wp.push(ptr);
}

//---------------------------------------------------------------
ResolveData DirectResolve::doRun()
{
    return synchronous_resolve(host.c_str(), srvc.c_str(), flags, family, type, proto);
}

void ReverseResolve::doRun(std::string& _rhost, std::string& _rsrvc)
{
    synchronous_resolve(_rhost, _rsrvc, addr, flags);
}

} //namespace aio;
} //namespace frame
} //namespace solid
