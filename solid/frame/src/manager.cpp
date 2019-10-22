// solid/frame/src/manager.cpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <deque>
#include <vector>

#include <climits>

#include <condition_variable>
#include <mutex>
#include <thread>

#include "solid/system/cassert.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/memory.hpp"
#include <atomic>

#include "solid/frame/actorbase.hpp"
#include "solid/frame/manager.hpp"
#include "solid/frame/reactorbase.hpp"
#include "solid/frame/service.hpp"

#include "solid/utility/event.hpp"
#include "solid/utility/queue.hpp"
#include "solid/utility/stack.hpp"

using namespace std;

namespace solid {
namespace frame {

const LoggerT logger("solid::frame");

//-----------------------------------------------------------------------------
namespace {

enum struct StatusE {
    Stopped,
    Running,
    Stopping,
};

enum {
    ErrorServiceUnknownE = 1,
    ErrorServiceNotRunningE,
    ErrorActorScheduleE
};

class ErrorCategory : public ErrorCategoryT {
public:
    ErrorCategory() {}

    const char* name() const noexcept override
    {
        return "solid::frame";
    }

    std::string message(int _ev) const override;
};

const ErrorCategory category;

std::string ErrorCategory::message(int _ev) const
{
    std::ostringstream oss;

    oss << "(" << name() << ":" << _ev << "): ";

    switch (_ev) {
    case 0:
        oss << "Success";
        break;
    case ErrorServiceUnknownE:
        oss << "Service not known";
        break;
    case ErrorServiceNotRunningE:
        oss << "Service not running";
        break;
    case ErrorActorScheduleE:
        oss << "Actor scheduling";
        break;
    default:
        oss << "Unknown";
        break;
    }
    return oss.str();
}
const ErrorConditionT error_service_unknown(ErrorServiceUnknownE, category);
const ErrorConditionT error_service_not_running(ErrorServiceNotRunningE, category);
const ErrorConditionT error_actor_schedule(ErrorActorScheduleE, category);

using StackSizeT = Stack<size_t>;

template <class T>
class Cache {
    std::mutex    mutex_;
    StackSizeT    index_stk_;
    std::deque<T> dq_;

public:
    T& create(size_t& _rindex)
    {
        lock_guard<mutex> lock(mutex_);
        if (!index_stk_.empty()) {
            _rindex = index_stk_.top();
            index_stk_.pop();
            return dq_[_rindex];
        } else {
            _rindex = dq_.size();
            dq_.emplace_back();
            return dq_.back();
        }
    }

    template <typename F>
    T& create(size_t& _rindex, const F& _rf)
    {
        lock_guard<mutex> lock(mutex_);
        if (!index_stk_.empty()) {
            _rindex = index_stk_.top();
            index_stk_.pop();
            T& rt = dq_[_rindex];
            _rf(_rindex, rt);
            return rt;
        } else {
            _rindex = dq_.size();
            dq_.emplace_back();
            _rf(_rindex, dq_.back());
            return dq_.back();
        }
    }

    void release(const size_t _index)
    {
        lock_guard<mutex> lock(mutex_);
        index_stk_.push(_index);
    }

    template <typename F>
    void release(const size_t _index, const F& _rf)
    { //Do not pass index as reference!!
        lock_guard<mutex> lock(mutex_);
        _rf(_index, dq_[_index]);
        index_stk_.push(_index);
    }

    template <typename F>
    T& aquire(const size_t _index, const F& _rf)
    {
        lock_guard<mutex> lock(mutex_);
        T&                rt = dq_[_index];
        _rf(_index, rt);
        return rt;
    }

    template <typename F>
    T* aquirePointer(const size_t _index, const F& _rf)
    {
        lock_guard<mutex> lock(mutex_);
        if (_index < dq_.size()) {
            T& rt = dq_[_index];
            _rf(_index, rt);
            return &rt;
        } else {
            return nullptr;
        }
    }
};

} //namespace
//-----------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& _ros, UniqueId const& _uid)
{
    _ros << _uid.index << ':' << _uid.unique;
    return _ros;
}

using SizeStackT    = Stack<size_t>;
using AtomicStatusT = std::atomic<StatusE>;

//-----------------------------------------------------------------------------
//POD structure
struct ActorStub {
    ActorBase*   pactor_;
    ReactorBase* preactor_;
    UniqueT      unique_;
};

struct ActorChunk {
    std::mutex& rmutex_;
    size_t      service_index_;
    size_t      next_chunk_;
    size_t      actor_count_;

    ActorChunk(std::mutex& _rmutex)
        : rmutex_(_rmutex)
        , service_index_(InvalidIndex())
        , next_chunk_(InvalidIndex())
        , actor_count_(0)
    {
    }

    void clear()
    {
        service_index_ = InvalidIndex();
        next_chunk_    = InvalidIndex();
        actor_count_   = 0;
    }

    char* data()
    {
        return reinterpret_cast<char*>(this);
    }
    ActorStub* actors()
    {
        return reinterpret_cast<ActorStub*>(reinterpret_cast<char*>(this) + sizeof(*this));
    }

    ActorStub& actor(const size_t _idx)
    {
        return actors()[_idx];
    }

    const ActorStub* actors() const
    {
        return reinterpret_cast<const ActorStub*>(reinterpret_cast<const char*>(this) + sizeof(*this));
    }

    ActorStub const& actor(const size_t _idx) const
    {
        return actors()[_idx];
    }

    std::mutex& mutex() const
    {
        return rmutex_;
    }
};

class ActorChunkStub {
    ActorChunk* pchunk_ = nullptr;

public:
    ActorChunk& chunk() const
    {
        return *pchunk_;
    }

    void chunk(ActorChunk* _pchunk)
    {
        if (pchunk_ != nullptr) {
            delete[] chunk().data();
        }
        pchunk_ = _pchunk;
    }

    bool empty() const
    {
        return pchunk_ == nullptr;
    }

    ~ActorChunkStub()
    {
        if (pchunk_ != nullptr) {
            delete[] chunk().data();
        }
    }
};

struct ServiceStub {

    Service*    pservice_;
    std::mutex* pmutex_;
    size_t      first_actor_chunk_;
    size_t      last_actor_chunk_;
    size_t      current_actor_index_;
    size_t      end_actor_index_;
    size_t      actor_count_;
    StatusE     status_;
    SizeStackT  actor_index_stk_;

    ServiceStub()
        : pservice_(nullptr)
        , pmutex_(nullptr)
        , first_actor_chunk_(InvalidIndex())
        , last_actor_chunk_(InvalidIndex())
        , current_actor_index_(InvalidIndex())
        , end_actor_index_(InvalidIndex())
        , actor_count_(0)
        , status_(StatusE::Stopped)
    {
    }

    void reset(Service* _pservice, const bool _start)
    {
        pservice_            = _pservice;
        first_actor_chunk_   = InvalidIndex();
        last_actor_chunk_    = InvalidIndex();
        current_actor_index_ = InvalidIndex();
        end_actor_index_     = InvalidIndex();
        actor_count_         = 0;
        status_              = _start ? StatusE::Running : StatusE::Stopped;

        while (!actor_index_stk_.empty()) {
            actor_index_stk_.pop();
        }
    }

    void reset()
    {
        solid_dbg(logger, Verbose, "" << pservice_);
        first_actor_chunk_   = InvalidIndex();
        last_actor_chunk_    = InvalidIndex();
        current_actor_index_ = InvalidIndex();
        end_actor_index_     = InvalidIndex();
        actor_count_         = 0;
        status_              = StatusE::Stopped;
        while (!actor_index_stk_.empty()) {
            actor_index_stk_.pop();
        }
    }

    std::mutex& mutex() const
    {
        return *pmutex_;
    }
};

//---------------------------------------------------------
struct Manager::Data {
    const size_t            service_mutex_count_;
    const size_t            actor_mutex_count_;
    const size_t            actor_chunk_size_;
    AtomicStatusT           status_;
    size_t                  service_count_;
    std::mutex*             pservice_mutex_array_;
    std::mutex*             pactor_mutex_array_;
    Cache<ServiceStub>      service_cache_;
    Cache<ActorChunkStub>   actor_chunk_cache_;
    std::mutex              mutex_;
    std::condition_variable condition_;

    Data(
        const size_t _service_mutex_count,
        const size_t _actor_mutex_count,
        const size_t _actor_chunk_size);

    ~Data();

    inline ActorChunk* allocateChunk(std::mutex& _rmtx) const
    {
        char*       pdata        = new char[sizeof(ActorChunk) + actor_chunk_size_ * sizeof(ActorStub)];
        ActorChunk* pactor_chunk = new (pdata) ActorChunk(_rmtx);
        for (size_t i = 0; i < actor_chunk_size_; ++i) {
            ActorStub& ras(pactor_chunk->actor(i));
            ras.pactor_   = nullptr;
            ras.preactor_ = nullptr;
            ras.unique_   = 0;
        }
        return pactor_chunk;
    }

    inline ActorChunkStub& chunk(const size_t _actor_index, std::unique_lock<std::mutex>& _rlock)
    {
        return actor_chunk_cache_.aquire(
            _actor_index / actor_chunk_size_,
            [&_rlock](const size_t, ActorChunkStub& _racs) {
                _rlock = std::unique_lock<std::mutex>(_racs.chunk().mutex());
            });
    }

    inline ActorChunkStub* chunkPointer(const size_t _actor_index, std::unique_lock<std::mutex>& _rlock)
    {
        return actor_chunk_cache_.aquirePointer(
            _actor_index / actor_chunk_size_,
            [&_rlock](const size_t, ActorChunkStub& _racs) {
                _rlock = std::unique_lock<std::mutex>(_racs.chunk().mutex());
            });
    }
};

Manager::Data::Data(
    const size_t _service_mutex_count,
    const size_t _actor_mutex_count,
    const size_t _actor_chunk_size)
    : service_mutex_count_(_service_mutex_count)
    , actor_mutex_count_(_actor_mutex_count)
    , actor_chunk_size_(_actor_chunk_size)
    , status_(StatusE::Running)
    , service_count_(0)
{
    pactor_mutex_array_ = new std::mutex[actor_mutex_count_];

    pservice_mutex_array_ = new std::mutex[service_mutex_count_];
}

Manager::Data::~Data()
{
    delete[] pactor_mutex_array_; //TODO: get rid of delete
    delete[] pservice_mutex_array_;
}

Manager::Manager(
    const size_t _service_mutex_count /* = 0*/,
    const size_t _actor_mutex_count /* = 0*/,
    const size_t _actor_chunk_size /* = 0*/
    )
    : impl_(make_pimpl<Data>(
        _service_mutex_count == 0 ? memory_page_size() / sizeof(std::mutex) : _service_mutex_count,
        _actor_mutex_count == 0 ? memory_page_size() / sizeof(std::mutex) : _actor_mutex_count,
        _actor_chunk_size == 0 ? (memory_page_size() - sizeof(ActorChunk)) / sizeof(ActorStub) : _actor_chunk_size))
{
    solid_dbg(logger, Verbose, "" << this);
}

/*virtual*/ Manager::~Manager()
{
    solid_dbg(logger, Verbose, "" << this);
    stop();
    solid_dbg(logger, Verbose, "" << this);
}

bool Manager::registerService(Service& _rservice, const bool _start)
{

    if (_rservice.registered()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(impl_->mutex_);

    if (impl_->status_.load() != StatusE::Running) {
        return false;
    }

    {
        size_t                       service_index = InvalidIndex();
        std::unique_lock<std::mutex> lock;
        ServiceStub&                 rss = impl_->service_cache_.create(
            service_index,
            [&lock, this](const size_t _index, ServiceStub& _rss) {
                if (_rss.pmutex_ == nullptr) {
                    _rss.pmutex_ = &impl_->pservice_mutex_array_[_index % impl_->service_mutex_count_];
                }
                lock = std::unique_lock<std::mutex>(_rss.mutex());
            });

        _rservice.index(service_index);

        rss.reset(&_rservice, _start);
    }
    ++impl_->service_count_;
    if (_start) {
        _rservice.statusSetRunning();
    }
    return true;
}

void Manager::unregisterService(Service& _rservice)
{
    const size_t service_index = _rservice.index();

    if (service_index == InvalidIndex()) {
        return;
    }

    std::lock_guard<std::mutex> lock(impl_->mutex_);

    doCleanupService(service_index, _rservice);

    impl_->service_cache_.release(service_index);

    --impl_->service_count_;
    if (impl_->service_count_ == 0 && impl_->status_.load() == StatusE::Stopping) {
        impl_->condition_.notify_all();
    }
}

void Manager::doCleanupService(const size_t _service_index, Service& _rservice)
{
    std::unique_lock<std::mutex> lock;
    ServiceStub&                 rss = impl_->service_cache_.aquire(
        _service_index,
        [&lock](const size_t /*_index*/, ServiceStub& _rss) {
            lock = std::unique_lock<std::mutex>(_rss.mutex());
        });

    if (rss.pservice_ == &_rservice) {

        rss.pservice_->index(InvalidIndex());

        size_t actor_chunk_index = rss.first_actor_chunk_;

        while (actor_chunk_index != InvalidIndex()) {
            impl_->actor_chunk_cache_.release(
                actor_chunk_index,
                [&actor_chunk_index](const size_t /*_index*/, const ActorChunkStub& _racs) {
                    actor_chunk_index = _racs.chunk().next_chunk_;
                });
        }

        rss.reset();
    }
}

ActorIdT Manager::registerActor(
    const Service&     _rservice,
    ActorBase&         _ractor,
    ReactorBase&       _rreactor,
    ScheduleFunctionT& _rschedule_fnc,
    ErrorConditionT&   _rerror)
{
    ActorIdT     retval;
    const size_t service_index = _rservice.index();

    if (service_index == InvalidIndex()) {
        _rerror = error_service_unknown;
        return retval;
    }

    size_t                       actor_index = InvalidIndex();
    std::unique_lock<std::mutex> lock;
    ServiceStub&                 rss = impl_->service_cache_.aquire(
        service_index,
        [&lock](const size_t _index, ServiceStub& _rss) {
            lock = std::unique_lock<std::mutex>(_rss.mutex());
        });

    if (rss.pservice_ != &_rservice || rss.status_ != StatusE::Running) {
        _rerror = error_service_not_running;
        return retval;
    }

    if (!rss.actor_index_stk_.empty()) {
        actor_index = rss.actor_index_stk_.top();
        rss.actor_index_stk_.pop();
    } else if (rss.current_actor_index_ < rss.end_actor_index_) {
        actor_index = rss.current_actor_index_;
        ++rss.current_actor_index_;
    } else {
        size_t actor_chunk_index = InvalidIndex();

        impl_->actor_chunk_cache_.create(
            actor_chunk_index,
            [this](const size_t _index, ActorChunkStub& _racs) {
                if (_racs.empty()) {
                    _racs.chunk(impl_->allocateChunk(impl_->pactor_mutex_array_[_index % impl_->actor_mutex_count_]));
                }
            });

        actor_index = actor_chunk_index * impl_->actor_chunk_size_;

        rss.current_actor_index_ = actor_index + 1;
        rss.end_actor_index_     = actor_index + impl_->actor_chunk_size_;

        if (rss.first_actor_chunk_ == InvalidIndex()) {
            rss.first_actor_chunk_ = rss.last_actor_chunk_ = actor_chunk_index;
        } else {
            {
                std::unique_lock<std::mutex> lock;
                ActorChunkStub&              rlast_chunk = impl_->actor_chunk_cache_.aquire(
                    rss.last_actor_chunk_,
                    [&lock](const size_t, ActorChunkStub& _racs) {
                        lock = std::unique_lock<std::mutex>(_racs.chunk().mutex());
                    });

                rlast_chunk.chunk().next_chunk_ = actor_chunk_index;
            }
            rss.last_actor_chunk_ = actor_chunk_index;
        }
    }
    {
        std::unique_lock<std::mutex> lock;
        ActorChunkStub&              racs   = impl_->chunk(actor_index, lock);
        ActorChunk&                  rchunk = racs.chunk();

        if (rchunk.service_index_ == InvalidIndex()) {
            rchunk.service_index_ = _rservice.index();
        }

        solid_assert(rchunk.service_index_ == _rservice.index());

        _ractor.id(actor_index);

        if (_rschedule_fnc(_rreactor)) {
            //the actor is scheduled
            //NOTE: a scheduler's reactor must ensure that the actor is fully
            //registered onto manager by locking the actor's mutex before calling
            //any the actor's code

            ActorStub& ras = rchunk.actor(actor_index % impl_->actor_chunk_size_);

            ras.pactor_   = &_ractor;
            ras.preactor_ = &_rreactor;
            retval.index  = actor_index;
            retval.unique = ras.unique_;
            ++rchunk.actor_count_;
            ++rss.actor_count_;
        } else {
            _ractor.id(InvalidIndex());
            rss.actor_index_stk_.push(actor_index);
            _rerror = error_actor_schedule;
        }
    }

    return retval;
}

void Manager::unregisterActor(ActorBase& _ractor)
{
    size_t service_index = InvalidIndex();
    size_t actor_index   = static_cast<size_t>(_ractor.id());
    {
        std::unique_lock<std::mutex> lock;
        ActorChunkStub&              racs   = impl_->chunk(actor_index, lock);
        ActorChunk&                  rchunk = racs.chunk();

        ActorStub& ras = rchunk.actor(actor_index % impl_->actor_chunk_size_);

        ras.pactor_   = nullptr;
        ras.preactor_ = nullptr;
        ++ras.unique_;

        _ractor.id(InvalidIndex());

        --rchunk.actor_count_;
        service_index = rchunk.service_index_;
        solid_assert(service_index != InvalidIndex());
    }
    {
        solid_assert(actor_index != InvalidIndex());

        std::unique_lock<std::mutex> lock;
        ServiceStub&                 rss = impl_->service_cache_.aquire(
            service_index,
            [&lock](const size_t _index, ServiceStub& _rss) {
                lock = std::unique_lock<std::mutex>(_rss.mutex());
            });

        rss.actor_index_stk_.push(actor_index);
        --rss.actor_count_;
        solid_dbg(logger, Verbose, "" << this << " serviceid = " << service_index << " actcnt = " << rss.actor_count_);
        if (rss.actor_count_ == 0 && rss.status_ == StatusE::Stopping) {
            impl_->condition_.notify_all();
        }
    }
}

bool Manager::disableActorVisits(ActorBase& _ractor)
{
    bool retval = false;

    if (_ractor.isRegistered()) {
        size_t                       actor_index = static_cast<size_t>(_ractor.id());
        std::unique_lock<std::mutex> lock;
        ActorChunkStub&              racs   = impl_->chunk(actor_index, lock);
        ActorChunk&                  rchunk = racs.chunk();

        ActorStub& ras = rchunk.actor(actor_index % impl_->actor_chunk_size_);
        retval         = (ras.preactor_ != nullptr);
        ras.preactor_  = nullptr;
    }
    return retval;
}

bool Manager::notify(ActorIdT const& _ruid, Event&& _uevt)
{
    const auto do_notify_fnc = [&_uevt](VisitContext& _rctx) {
        return _rctx.raiseActor(std::move(_uevt));
    };

    return doVisit(_ruid, ActorVisitFunctionT{do_notify_fnc});
}

size_t Manager::notifyAll(const Service& _rsvc, Event const& _revt)
{
    const auto do_notify_fnc = [_revt](VisitContext& _rctx) {
        return _rctx.raiseActor(_revt);
    };
    return doForEachServiceActor(_rsvc, ActorVisitFunctionT{do_notify_fnc});
}

bool Manager::doVisit(ActorIdT const& _actor_id, const ActorVisitFunctionT _rfct)
{

    bool retval = false;

    std::unique_lock<std::mutex> lock;
    ActorChunkStub*              pacs = impl_->chunkPointer(_actor_id.index, lock);
    if (pacs != nullptr) {
        ActorChunk& rchunk = pacs->chunk();
        ActorStub&  ras    = rchunk.actor(_actor_id.index % impl_->actor_chunk_size_);

        if (ras.unique_ == _actor_id.unique && ras.pactor_ != nullptr && ras.preactor_ != nullptr) {
            VisitContext ctx(*this, *ras.preactor_, *ras.pactor_);
            retval = _rfct(ctx);
        }
    }
    return retval;
}

std::mutex& Manager::mutex(const ActorBase& _ractor) const
{
    size_t                       actor_index = static_cast<size_t>(_ractor.id());
    std::unique_lock<std::mutex> lock;
    ActorChunkStub&              racs   = impl_->chunk(actor_index, lock);
    ActorChunk&                  rchunk = racs.chunk();

    return rchunk.mutex();
}

ActorIdT Manager::id(const ActorBase& _ractor) const
{
    const IndexT actor_index = _ractor.id();
    ActorIdT     retval;

    if (actor_index != InvalidIndex()) {
        std::unique_lock<std::mutex> lock;
        ActorChunkStub&              racs   = impl_->chunk(actor_index, lock);
        ActorChunk&                  rchunk = racs.chunk();
        ActorStub&                   ras    = rchunk.actor(actor_index % impl_->actor_chunk_size_);

        ActorBase* pactor = ras.pactor_;
        solid_assert(pactor == &_ractor);
        retval = ActorIdT(actor_index, ras.unique_);
    }
    return retval;
}

std::mutex& Manager::mutex(const Service& _rservice) const
{
    std::mutex*  pmutex        = nullptr;
    const size_t service_index = _rservice.index();

    impl_->service_cache_.aquire(
        service_index,
        [&pmutex](const size_t _index, ServiceStub& _rss) {
            pmutex = &_rss.mutex();
        });

    return *pmutex;
}

size_t Manager::doForEachServiceActor(const Service& _rservice, const ActorVisitFunctionT _rvisit_fnc)
{
    if (!_rservice.registered()) {
        return 0u;
    }
    const size_t service_index     = _rservice.index();
    size_t       first_actor_chunk = InvalidIndex();
    {
        std::unique_lock<std::mutex> lock;
        ServiceStub&                 rss = impl_->service_cache_.aquire(
            service_index,
            [&lock](const size_t _index, ServiceStub& _rss) {
                lock = std::unique_lock<std::mutex>(_rss.mutex());
            });
        first_actor_chunk = rss.first_actor_chunk_;
    }

    return doForEachServiceActor(first_actor_chunk, _rvisit_fnc);
}

size_t Manager::doForEachServiceActor(const size_t _first_actor_chunk, const ActorVisitFunctionT _rvisit_fnc)
{

    size_t chunk_index   = _first_actor_chunk;
    size_t visited_count = 0;

    while (chunk_index != InvalidIndex()) {

        std::unique_lock<std::mutex> lock;
        ActorChunkStub&              racs    = impl_->chunk(chunk_index * impl_->actor_chunk_size_, lock);
        ActorChunk&                  rchunk  = racs.chunk();
        ActorStub*                   pactors = rchunk.actors();

        for (size_t i(0), cnt(0); i < impl_->actor_chunk_size_ && cnt < rchunk.actor_count_; ++i) {
            ActorStub& ractor = pactors[i];
            if (ractor.pactor_ != nullptr && ractor.preactor_ != nullptr) {
                VisitContext ctx(*this, *ractor.preactor_, *ractor.pactor_);
                _rvisit_fnc(ctx);
                ++visited_count;
                ++cnt;
            }
        }

        chunk_index = rchunk.next_chunk_;
    }

    return visited_count;
}

Service& Manager::service(const ActorBase& _ractor) const
{

    Service* pservice = nullptr;

    size_t service_index = InvalidIndex();
    {
        size_t                       actor_index = static_cast<size_t>(_ractor.id());
        std::unique_lock<std::mutex> lock;
        ActorChunkStub&              racs   = impl_->chunk(actor_index, lock);
        ActorChunk&                  rchunk = racs.chunk();

        service_index = rchunk.service_index_;
    }

    {
        std::unique_lock<std::mutex> lock;
        ServiceStub&                 rss = impl_->service_cache_.aquire(
            service_index,
            [&lock](const size_t _index, ServiceStub& _rss) {
                lock = std::unique_lock<std::mutex>(_rss.mutex());
            });

        pservice = rss.pservice_;
    }
    return *pservice;
}

void Manager::doStartService(Service& _rservice, const LockedFunctionT& _locked_fnc, const UnlockedFunctionT& _unlocked_fnc)
{
    solid_check(_rservice.registered(), "Service not registered");

    {
        const size_t                 service_index = _rservice.index();
        std::unique_lock<std::mutex> lock;
        ServiceStub&                 rss = impl_->service_cache_.aquire(
            service_index,
            [&lock](const size_t _index, ServiceStub& _rss) {
                lock = std::unique_lock<std::mutex>(_rss.mutex());
            });

        solid_check(rss.status_ == StatusE::Stopped, "Service not stopped");

        rss.status_ = StatusE::Running;

        try {
            _locked_fnc();
        } catch (...) {
            rss.status_ = StatusE::Stopped;
            throw;
        }
    }
    try {
        _unlocked_fnc();
    } catch (...) {
        stopService(_rservice, true);
        throw;
    }
    _rservice.statusSetRunning();
}

void Manager::stopService(Service& _rservice, const bool _wait)
{
    solid_check(_rservice.registered(), "Service not registered");

    const size_t                 service_index = _rservice.index();
    std::unique_lock<std::mutex> lock;
    ServiceStub&                 rss = impl_->service_cache_.aquire(
        service_index,
        [&lock](const size_t _index, ServiceStub& _rss) {
            lock = std::unique_lock<std::mutex>(_rss.mutex());
        });

    solid_dbg(logger, Verbose, "" << service_index << " actor_count = " << rss.actor_count_);

    _rservice.statusSetStopping();

    if (rss.status_ == StatusE::Running) {
        rss.status_             = StatusE::Stopping;
        const auto raise_lambda = [](VisitContext& _rctx) {
            return _rctx.raiseActor(make_event(GenericEvents::Kill));
        };
        const size_t cnt = doForEachServiceActor(rss.first_actor_chunk_, ActorVisitFunctionT{raise_lambda});

        if (cnt == 0 && rss.actor_count_ == 0) {
            solid_dbg(logger, Verbose, "StateStoppedE on " << service_index);
            rss.status_ = StatusE::Stopped;
            _rservice.statusSetStopped();
            return;
        }
    }

    if (rss.status_ == StatusE::Stopped) {
        solid_dbg(logger, Verbose, "" << service_index);
        return;
    }

    if (rss.status_ == StatusE::Stopping && _wait) {
        while (rss.actor_count_ != 0u) {
            impl_->condition_.wait(lock);
        }
        rss.status_ = StatusE::Stopped;
        _rservice.statusSetStopped();
    }
}

void Manager::start()
{
    {
        std::unique_lock<std::mutex> lock(impl_->mutex_);

        while (impl_->status_ != StatusE::Running) {
            if (impl_->status_ == StatusE::Stopped) {
                impl_->status_ = StatusE::Running;
                continue;
            }
            if (impl_->status_ == StatusE::Running) {
                continue;
            }

            if (impl_->status_ == StatusE::Stopping) {
                while (impl_->status_ == StatusE::Stopping) {
                    impl_->condition_.wait(lock);
                }
            } else {
                continue;
            }
        }
    }
}

void Manager::stop()
{
    {
        std::unique_lock<std::mutex> lock(impl_->mutex_);
        if (impl_->status_ == StatusE::Stopped) {
            return;
        }
        if (impl_->status_ == StatusE::Running) {
            impl_->status_ = StatusE::Stopping;
        } else if (impl_->status_ == StatusE::Stopping) {
            while (impl_->service_count_ != 0u) {
                impl_->condition_.wait(lock);
            }
        } else {
            return;
        }
    }

    const auto raise_lambda = [](VisitContext& _rctx) {
        return _rctx.raiseActor(make_event(GenericEvents::Kill));
    };

    //broadcast to all actors to stop
    for (size_t service_index = 0; true; ++service_index) {
        std::unique_lock<std::mutex> lock;
        ServiceStub*                 pss = impl_->service_cache_.aquirePointer(
            service_index,
            [&lock](const size_t _index, ServiceStub& _rss) {
                lock = std::unique_lock<std::mutex>(_rss.mutex());
            });

        if (pss != nullptr) {

            if (pss->pservice_ != nullptr && pss->status_ == StatusE::Running) {
                pss->status_     = StatusE::Stopping;
                const size_t cnt = doForEachServiceActor(pss->first_actor_chunk_, ActorVisitFunctionT{raise_lambda});
                (void)cnt;

                if (pss->actor_count_ == 0) {
                    pss->status_ = StatusE::Stopped;
                    pss->pservice_->statusSetStopped();
                    solid_dbg(logger, Verbose, "StateStoppedE on " << service_index);
                }
            }
        } else {
            break;
        }
    } //for

    //wait for all services to stop
    for (size_t service_index = 0; true; ++service_index) {
        std::unique_lock<std::mutex> lock;
        ServiceStub*                 pss = impl_->service_cache_.aquirePointer(
            service_index,
            [&lock](const size_t _index, ServiceStub& _rss) {
                lock = std::unique_lock<std::mutex>(_rss.mutex());
            });

        if (pss != nullptr) {

            if (pss->pservice_ != nullptr && pss->status_ == StatusE::Stopping) {
                solid_dbg(logger, Verbose, "wait stop service: " << service_index);
                while (pss->actor_count_ != 0u) {
                    impl_->condition_.wait(lock);
                }
                pss->status_ = StatusE::Stopped;
                pss->pservice_->statusSetStopped();
                solid_dbg(logger, Verbose, "StateStoppedE on " << service_index);
            }
        } else {
            break;
        }
    } //for

    {
        std::lock_guard<std::mutex> lock(impl_->mutex_);
        impl_->status_ = StatusE::Stopped;
        impl_->condition_.notify_all();
    }
}
//-----------------------------------------------------------------------------
bool Manager::VisitContext::raiseActor(Event const& _revent) const
{
    return rreactor_.raise(actor().runId(), _revent);
}

bool Manager::VisitContext::raiseActor(Event&& _uevent) const
{
    return rreactor_.raise(actor().runId(), std::move(_uevent));
}
//-----------------------------------------------------------------------------

} //namespace frame
} //namespace solid
