// solid/frame/src/manager.cpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
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

const LoggerT frame_logger("solid::frame");

//-----------------------------------------------------------------------------
namespace {
enum struct StatusE {
    Stopped,
    // Starting,
    Running,
    Stopping,
};
enum {
    ErrorServiceUnknownE = 1,
    ErrorServiceNotRunningE,
    ErrorActorScheduleE,
    ErrorManagerFullE
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
    case ErrorManagerFullE:
        oss << "Manager full";
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
const ErrorConditionT error_manager_full(ErrorManagerFullE, category);

using StackSizeT = Stack<size_t>;

template <class T>
class Store {
    StackSizeT     index_stk_;
    atomic<size_t> data_size_;
    vector<T>      data_vec_;

public:
    Store(const size_t _capacity)
        : data_size_(0)
        , data_vec_(_capacity)
    {
    }

    T* create(size_t& _rindex)
    {
        if (!index_stk_.empty()) {
            _rindex = index_stk_.top();
            index_stk_.pop();
        } else if (data_size_.load() < data_vec_.size()) {
            _rindex = data_size_.fetch_add(1);
        } else {
            return nullptr;
        }
        return &data_vec_[_rindex];
    }

    template <typename F>
    T* create(size_t& _rindex, const F& _rf)
    {
        if (!index_stk_.empty()) {
            _rindex = index_stk_.top();
            index_stk_.pop();
            return &data_vec_[_rindex];
        } else if (data_size_.load() < data_vec_.size()) {
            _rindex = data_size_.load();
            T& rt   = data_vec_[_rindex];
            _rf(std::cref(_rindex), rt);
            ++data_size_;
            return &rt;
        } else {
            return nullptr;
        }
    }

    void release(const size_t _index)
    {
        index_stk_.push(_index);
    }

    template <typename F>
    void release(const size_t _index, const F& _rf)
    { // Do not pass index as reference!!
        _rf(_index, data_vec_[_index]);
        index_stk_.push(_index);
    }

    template <typename F>
    const T& aquire(const size_t _index, const F& _rf) const
    {
        solid_assert(_index < data_size_);
        const T& rt = data_vec_[_index];
        _rf(_index, rt);
        return rt;
    }

    template <typename F>
    const T* aquirePointer(const size_t _index, const F& _rf) const
    {
        if (_index < data_size_.load()) {
            const T& rt = data_vec_[_index];
            _rf(_index, rt);
            return &rt;
        } else {
            return nullptr;
        }
    }

    template <typename F>
    T& aquire(const size_t _index, const F& _rf)
    {
        solid_assert(_index < data_size_);
        T& rt = data_vec_[_index];
        _rf(_index, rt);
        return rt;
    }

    template <typename F>
    T* aquirePointer(const size_t _index, const F& _rf)
    {
        if (_index < data_size_.load()) {
            T& rt = data_vec_[_index];
            _rf(_index, rt);
            return &rt;
        } else {
            return nullptr;
        }
    }
};

} // namespace
//-----------------------------------------------------------------------------

std::ostream& operator<<(std::ostream& _ros, UniqueId const& _uid)
{
    _ros << _uid.index << ':' << _uid.unique;
    return _ros;
}

using SizeStackT    = Stack<size_t>;
using AtomicStatusT = std::atomic<StatusE>;

//-----------------------------------------------------------------------------
// POD structure
struct ActorStub {
    ActorBase*      pactor_;
    ReactorBasePtrT reactor_ptr_;
    UniqueT         unique_;
};

struct ActorChunk {
    std::byte*            this_buf_;
    Manager::ChunkMutexT& rmutex_;
    size_t                service_index_;
    size_t                next_chunk_;
    size_t                actor_count_;
    ActorStub             sentinel_[1];

    static ActorChunk* allocate(const size_t _chunk_size, Manager::ChunkMutexT& _rmtx)
    {
        auto* pdata  = new std::byte[sizeof(ActorChunk) + (_chunk_size - 1) * sizeof(ActorStub)];
        auto* pchunk = new (pdata) ActorChunk(pdata, _rmtx);
        for (size_t i = 1; i < _chunk_size; ++i) {
            new (pchunk->actors() + i) ActorStub;
        }
        return pchunk;
    }

    ActorChunk(std::byte* _this_buf, Manager::ChunkMutexT& _rmutex)
        : this_buf_(_this_buf)
        , rmutex_(_rmutex)
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

    std::byte* data()
    {
        return this_buf_;
    }

    ActorStub* actors()
    {
        return sentinel_;
    }

    ActorStub& operator[](const size_t _idx)
    {
        return actors()[_idx];
    }

    const ActorStub* actors() const
    {
        return sentinel_;
    }

    ActorStub const& operator[](const size_t _idx) const
    {
        return actors()[_idx];
    }

    Manager::ActorMutexT& mutex() const
    {
        return rmutex_;
    }
};

struct ActorChunkDeleter {
    void operator()(ActorChunk* _pchunk)
    {
        if (_pchunk) {
            delete[] _pchunk->data();
        }
    }
};

using ActorChunkPtrT = std::unique_ptr<ActorChunk, ActorChunkDeleter>;

struct ServiceStub {

    Service*                    pservice_;
    Manager::ServiceMutexT*     pmutex_;
    size_t                      first_actor_chunk_;
    size_t                      last_actor_chunk_;
    size_t                      current_actor_index_;
    size_t                      end_actor_index_;
    size_t                      actor_count_;
    std::atomic<ServiceStatusE> status_;
    SizeStackT                  actor_index_stk_;

    ServiceStub()
        : pservice_(nullptr)
        , pmutex_(nullptr)
        , first_actor_chunk_(InvalidIndex())
        , last_actor_chunk_(InvalidIndex())
        , current_actor_index_(InvalidIndex())
        , end_actor_index_(InvalidIndex())
        , actor_count_(0)
        , status_(ServiceStatusE::Stopped)
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
        status_              = _start ? ServiceStatusE::Running : ServiceStatusE::Stopped;

        while (!actor_index_stk_.empty()) {
            actor_index_stk_.pop();
        }
    }

    void reset()
    {
        solid_log(frame_logger, Verbose, "" << pservice_);
        first_actor_chunk_   = InvalidIndex();
        last_actor_chunk_    = InvalidIndex();
        current_actor_index_ = InvalidIndex();
        end_actor_index_     = InvalidIndex();
        actor_count_         = 0;
        status_              = ServiceStatusE::Stopped;
        while (!actor_index_stk_.empty()) {
            actor_index_stk_.pop();
        }
    }

    Manager::ServiceMutexT& mutex() const
    {
        return *pmutex_;
    }
};

//---------------------------------------------------------
struct Manager::Data {
    const size_t                service_capacity_;
    const size_t                actor_capacity_;
    const size_t                actor_chunk_size_;
    const size_t                service_mutex_count_;
    const size_t                actor_mutex_count_;
    const size_t                chunk_mutex_count_;
    AtomicStatusT               status_;
    atomic<size_t>              size_;
    size_t                      service_count_;
    std::mutex                  mutex_;
    std::condition_variable     condition_;
    unique_ptr<ServiceMutexT[]> service_mutex_array_ptr_;
    unique_ptr<ActorMutexT[]>   actor_mutex_array_ptr_;
    unique_ptr<ChunkMutexT[]>   chunk_mutex_array_ptr_;
    Store<ServiceStub>          service_store_;
    Store<ActorChunkPtrT>       actor_chunk_store_;

    Data(
        const size_t _service_capacity,
        const size_t _actor_capacity,
        const size_t _actor_bucket_size,
        const size_t _service_mutex_count,
        const size_t _actor_mutex_count,
        const size_t _chunk_mutex_count)
        : service_capacity_(_service_capacity)
        , actor_capacity_(_actor_capacity)
        , actor_chunk_size_(_actor_bucket_size)
        , service_mutex_count_(_service_mutex_count)
        , actor_mutex_count_(_actor_mutex_count)
        , chunk_mutex_count_(_chunk_mutex_count)
        , status_(StatusE::Running)
        , size_(0)
        , service_count_(0)
        , service_store_(_service_capacity)
        , actor_chunk_store_(_actor_capacity / _actor_bucket_size)
    {
        actor_mutex_array_ptr_.reset(new ActorMutexT[actor_mutex_count_]);
        service_mutex_array_ptr_.reset(new ServiceMutexT[service_mutex_count_]);
        chunk_mutex_array_ptr_.reset(new ChunkMutexT[chunk_mutex_count_]);
    }

    ~Data();

    inline ActorChunk* allocateChunk(ChunkMutexT& _rmtx) const
    {
        ActorChunk& ractor_chunk = *ActorChunk::allocate(actor_chunk_size_, _rmtx);

        for (size_t i = 0; i < actor_chunk_size_; ++i) {
            ActorStub& ras(ractor_chunk[i]);
            ras.pactor_ = nullptr;
            ras.reactor_ptr_.reset();
            ras.unique_ = 0;
        }
        return &ractor_chunk;
    }

    inline ActorChunkPtrT const& chunk(const size_t _actor_index, std::unique_lock<ChunkMutexT>& _rlock) const
    {
        return actor_chunk_store_.aquire(
            _actor_index / actor_chunk_size_,
            [&_rlock](const size_t, const ActorChunkPtrT& _chunk_ptr) {
                _rlock = std::unique_lock<ChunkMutexT>(_chunk_ptr->mutex());
            });
    }

    inline ActorChunkPtrT const& chunk(const size_t _actor_index) const
    {
        return actor_chunk_store_.aquire(
            _actor_index / actor_chunk_size_,
            [](const size_t, const ActorChunkPtrT&) {
            });
    }

    inline ActorChunkPtrT& chunk(const size_t _actor_index, std::unique_lock<ChunkMutexT>& _rlock)
    {
        return actor_chunk_store_.aquire(
            _actor_index / actor_chunk_size_,
            [&_rlock](const size_t, ActorChunkPtrT& _chunk_ptr) {
                _rlock = std::unique_lock<ChunkMutexT>(_chunk_ptr->mutex());
            });
    }

    inline ActorChunkPtrT& chunk(const size_t _actor_index)
    {
        return actor_chunk_store_.aquire(
            _actor_index / actor_chunk_size_,
            [](const size_t, ActorChunkPtrT&) {
            });
    }

    inline ActorChunkPtrT* chunkPointer(const size_t _actor_index, std::unique_lock<ChunkMutexT>& _rlock)
    {
        return actor_chunk_store_.aquirePointer(
            _actor_index / actor_chunk_size_,
            [&_rlock](const size_t, ActorChunkPtrT& _chunk_ptr) {
                _rlock = std::unique_lock<ChunkMutexT>(_chunk_ptr->mutex());
            });
    }
    inline ActorChunkPtrT* chunkPointer(const size_t _actor_index)
    {
        return actor_chunk_store_.aquirePointer(
            _actor_index / actor_chunk_size_,
            [](const size_t, ActorChunkPtrT& _chunk_ptr) {
            });
    }
    inline ActorMutexT& actorMutex(const size_t _actor_index)
    {
        return actor_mutex_array_ptr_[_actor_index % actor_mutex_count_];
    }

    inline const ServiceStub& service(const size_t _service_index, std::unique_lock<ServiceMutexT>& _rlock) const
    {
        return service_store_.aquire(
            _service_index,
            [&_rlock](const size_t _index, const ServiceStub& _rss) {
                _rlock = std::unique_lock<ServiceMutexT>(_rss.mutex());
            });
    }
    inline const ServiceStub* servicePointer(const size_t _service_index, std::unique_lock<ServiceMutexT>& _rlock) const
    {
        return service_store_.aquirePointer(
            _service_index,
            [&_rlock](const size_t _index, const ServiceStub& _rss) {
                _rlock = std::unique_lock<ServiceMutexT>(_rss.mutex());
            });
    }

    inline ServiceStub& service(const size_t _service_index, std::unique_lock<ServiceMutexT>& _rlock)
    {
        return service_store_.aquire(
            _service_index,
            [&_rlock](const size_t _index, ServiceStub& _rss) {
                _rlock = std::unique_lock<ServiceMutexT>(_rss.mutex());
            });
    }

    inline ServiceStub* servicePointer(const size_t _service_index, std::unique_lock<ServiceMutexT>& _rlock)
    {
        return service_store_.aquirePointer(
            _service_index,
            [&_rlock](const size_t _index, ServiceStub& _rss) {
                _rlock = std::unique_lock<ServiceMutexT>(_rss.mutex());
            });
    }
};

Manager::Data::~Data()
{
}

Manager::Manager(
    const size_t _service_capacity,
    const size_t _actor_capacity,
    const size_t _actor_bucket_size,
    const size_t _service_mutex_count,
    const size_t _actor_mutex_count,
    const size_t _chunk_mutex_count)
    : pimpl_(
          _service_capacity,
          _actor_capacity,
          _actor_bucket_size == 0 ? (memory_page_size() - sizeof(ActorChunk) + sizeof(ActorStub)) / sizeof(ActorStub) : _actor_bucket_size,
          _service_mutex_count == 0 ? _service_capacity : _service_mutex_count,
          _actor_mutex_count == 0 ? 1024 : _actor_mutex_count,
          _chunk_mutex_count == 0 ? 1024 : _chunk_mutex_count)
{
    solid_log(frame_logger, Verbose, "" << this);
}

/*virtual*/ Manager::~Manager()
{
    solid_log(frame_logger, Verbose, "" << this);
    stop();
    solid_log(frame_logger, Verbose, "" << this);
}

bool Manager::registerService(Service& _rservice, const bool _start)
{

    if (_rservice.registered()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(pimpl_->mutex_);

    if (pimpl_->status_.load() != StatusE::Running) {
        return false;
    }

    {
        size_t                          service_index = InvalidIndex();
        std::unique_lock<ServiceMutexT> service_lock;
        ServiceStub*                    pss = pimpl_->service_store_.create(
            service_index,
            [&service_lock, this](const size_t _index, ServiceStub& _rss) {
                if (_rss.pmutex_ == nullptr) {
                    _rss.pmutex_ = &pimpl_->service_mutex_array_ptr_[_index % pimpl_->service_mutex_count_];
                }
                service_lock = std::unique_lock<ServiceMutexT>(_rss.mutex());
            });
        if (pss) {
            _rservice.index(service_index);
            pss->reset(&_rservice, _start);
        } else {
            return false;
        }
    }
    ++pimpl_->service_count_;
    return true;
}

void Manager::unregisterService(Service& _rservice)
{
    const size_t service_index = _rservice.index();

    if (service_index == InvalidIndex()) {
        return;
    }

    std::lock_guard<std::mutex> lock(pimpl_->mutex_);

    doCleanupService(service_index, _rservice);

    pimpl_->service_store_.release(service_index);

    --pimpl_->service_count_;
    if (pimpl_->service_count_ == 0 && pimpl_->status_.load() == StatusE::Stopping) {
        pimpl_->condition_.notify_all();
    }
}

void Manager::doCleanupService(const size_t _service_index, Service& _rservice)
{
    std::unique_lock<ServiceMutexT> service_lock;
    ServiceStub&                    rss = pimpl_->service(_service_index, service_lock);

    if (rss.pservice_ == &_rservice) {

        rss.pservice_->index(InvalidIndex());

        size_t actor_chunk_index = rss.first_actor_chunk_;

        while (actor_chunk_index != InvalidIndex()) {
            pimpl_->actor_chunk_store_.release(
                actor_chunk_index,
                [&actor_chunk_index](const size_t /*_index*/, const ActorChunkPtrT& _rchunk_ptr) {
                    actor_chunk_index = _rchunk_ptr->next_chunk_;
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

    size_t                          actor_index = InvalidIndex();
    std::unique_lock<ServiceMutexT> service_lock;
    ServiceStub&                    rss = pimpl_->service(service_index, service_lock);

    if (rss.pservice_ != &_rservice || rss.status_ != ServiceStatusE::Running) {
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

        auto* pacs = pimpl_->actor_chunk_store_.create(
            actor_chunk_index,
            [this](const size_t _index, ActorChunkPtrT& _rchunk_ptr) {
                if (!_rchunk_ptr) {
                    _rchunk_ptr.reset(pimpl_->allocateChunk(pimpl_->chunk_mutex_array_ptr_[_index % pimpl_->chunk_mutex_count_]));
                }
            });

        if (pacs == nullptr) {
            _rerror = error_manager_full;
            return retval;
        }

        actor_index = actor_chunk_index * pimpl_->actor_chunk_size_;

        rss.current_actor_index_ = actor_index + 1;
        rss.end_actor_index_     = actor_index + pimpl_->actor_chunk_size_;

        if (rss.first_actor_chunk_ == InvalidIndex()) {
            rss.first_actor_chunk_ = rss.last_actor_chunk_ = actor_chunk_index;
        } else {
            {
                std::unique_lock<ChunkMutexT> chunk_lock;
                ActorChunkPtrT&               rlast_chunk = pimpl_->actor_chunk_store_.aquire(
                    rss.last_actor_chunk_,
                    [&chunk_lock](const size_t, ActorChunkPtrT& _rchunk_ptr) {
                        chunk_lock = std::unique_lock<ChunkMutexT>(_rchunk_ptr->mutex());
                    });

                rlast_chunk->next_chunk_ = actor_chunk_index;
            }
            rss.last_actor_chunk_ = actor_chunk_index;
        }
    }
    {
        std::unique_lock<ChunkMutexT> chunk_lock;
        ActorChunkPtrT&               rchunk_ptr = pimpl_->chunk(actor_index, chunk_lock);
        ActorChunk&                   rchunk     = *rchunk_ptr;

        if (rchunk.service_index_ == InvalidIndex()) {
            rchunk.service_index_ = _rservice.index();
        }

        solid_assert_log(rchunk.service_index_ == _rservice.index(), frame_logger);

        // std::lock_guard<std::mutex> actor_lock{pimpl_->actorMutex(actor_index)};

        _ractor.id(actor_index);

        if (_rschedule_fnc(_rreactor)) {
            // the actor is scheduled
            // NOTE: a scheduler's reactor must ensure that the actor is fully
            // registered onto manager by locking the actor's mutex before calling
            // any the actor's code

            ActorStub& ras = rchunk[actor_index % pimpl_->actor_chunk_size_];

            ras.pactor_      = &_ractor;
            ras.reactor_ptr_ = _rreactor.shared_from_this();
            retval.index     = actor_index;
            retval.unique    = ras.unique_;
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
    size_t       service_index = InvalidIndex();
    const size_t actor_index   = static_cast<size_t>(_ractor.id());
    {
        std::unique_lock<ChunkMutexT> chunk_lock;
        ActorChunkPtrT&               rchunk_ptr = pimpl_->chunk(actor_index, chunk_lock);
        ActorChunk&                   rchunk     = *rchunk_ptr;

        // std::lock_guard<std::mutex> actor_lock{pimpl_->actorMutex(actor_index)};
        ActorStub& ras = rchunk[actor_index % pimpl_->actor_chunk_size_];

        ras.pactor_ = nullptr;
        ras.reactor_ptr_.reset();
        ++ras.unique_;

        _ractor.id(InvalidIndex());

        --rchunk.actor_count_;
        service_index = rchunk.service_index_;
        solid_assert_log(service_index != InvalidIndex(), frame_logger);
    }
    {
        solid_assert_log(actor_index != InvalidIndex(), frame_logger);

        std::unique_lock<std::mutex> service_lock;
        ServiceStub&                 rss = pimpl_->service(service_index, service_lock);

        rss.actor_index_stk_.push(actor_index);
        --rss.actor_count_;
        solid_log(frame_logger, Verbose, "" << this << " serviceid = " << service_index << " actcnt = " << rss.actor_count_);
        if (rss.actor_count_ == 0 && rss.status_ == ServiceStatusE::Stopping) {
            pimpl_->condition_.notify_all();
        }
    }
}

bool Manager::disableActorVisits(ActorBase& _ractor)
{
    bool retval = false;

    if (_ractor.isRegistered()) {
        const size_t                  actor_index = static_cast<size_t>(_ractor.id());
        std::unique_lock<ChunkMutexT> chunk_lock;
        ActorChunkPtrT&               rchunc_ptr = pimpl_->chunk(actor_index, chunk_lock);
        ActorChunk&                   rchunk     = *rchunc_ptr;
        // std::lock_guard<std::mutex> actor_lock{pimpl_->actorMutex(actor_index)};

        ActorStub& ras = rchunk[actor_index % pimpl_->actor_chunk_size_];
        retval         = (ras.reactor_ptr_.get() != nullptr);
        ras.reactor_ptr_.reset();
    }
    return retval;
}

bool Manager::notify(ActorIdT const& _ruid, EventBase&& _uevt)
{
    const auto do_notify_fnc = [&_uevt](VisitContext& _rctx) {
        return _rctx.wakeActor(std::move(_uevt));
    };

    return doVisit(_ruid, ActorVisitFunctionT{do_notify_fnc});
}

bool Manager::notify(ActorIdT const& _ruid, const EventBase& _evt)
{
    const auto do_notify_fnc = [&_evt](VisitContext& _rctx) {
        return _rctx.wakeActor(_evt);
    };

    return doVisit(_ruid, ActorVisitFunctionT{do_notify_fnc});
}

size_t Manager::notifyAll(const Service& _rsvc, EventBase const& _revt)
{
    const auto do_notify_fnc = [&_revt](VisitContext& _rctx) {
        return _rctx.wakeActor(_revt);
    };
    return doForEachServiceActor(_rsvc, ActorVisitFunctionT{do_notify_fnc});
}

bool Manager::doVisit(ActorIdT const& _actor_id, const ActorVisitFunctionT& _rfct)
{

    bool                          retval = false;
    std::unique_lock<ChunkMutexT> chunk_lock;
    ActorChunkPtrT*               pchunk_ptr = pimpl_->chunkPointer(_actor_id.index, chunk_lock);
    if (pchunk_ptr != nullptr) {
        ActorChunk& rchunk = *(*pchunk_ptr);
        ActorStub&  ras    = rchunk[_actor_id.index % pimpl_->actor_chunk_size_];

        if (ras.unique_ == _actor_id.unique && ras.pactor_ != nullptr && ras.reactor_ptr_) {
            VisitContext ctx(*this, ras.reactor_ptr_, ras.pactor_->runId());
            chunk_lock.unlock();

            retval = _rfct(ctx);
        }
    }
    return retval;
}

Manager::ActorMutexT& Manager::mutex(const ActorBase& _ractor) const
{
    ChunkMutexT* pmutex = nullptr;
    pimpl_->actor_chunk_store_.aquirePointer(
        static_cast<size_t>(_ractor.id()) / pimpl_->actor_chunk_size_,
        [&pmutex](const size_t, const ActorChunkPtrT& _chunk_ptr) {
            pmutex = &_chunk_ptr->mutex();
        });
    return *pmutex;
}

ActorIdT Manager::id(const ActorBase& _ractor) const
{
    const IndexT actor_index = _ractor.id();
    ActorIdT     retval;

    if (actor_index != InvalidIndex()) {
        std::unique_lock<ChunkMutexT> chunk_lock;
        const ActorChunkPtrT&         rchunk_ptr = pimpl_->chunk(actor_index, chunk_lock);
        const ActorChunk&             rchunk     = *rchunk_ptr;
        // std::lock_guard<std::mutex> actor_lock{pimpl_->actorMutex(actor_index)};
        const ActorStub& ras = rchunk[actor_index % pimpl_->actor_chunk_size_];

        const ActorBase* pactor = ras.pactor_;
        solid_assert_log(pactor == &_ractor, frame_logger);
        retval = ActorIdT(actor_index, ras.unique_);
    }
    return retval;
}

Manager::ServiceMutexT& Manager::mutex(const Service& _rservice) const
{
    ServiceMutexT* pmutex        = nullptr;
    const size_t   service_index = _rservice.index();

    pimpl_->service_store_.aquire(
        service_index,
        [&pmutex](const size_t _index, const ServiceStub& _rss) {
            pmutex = &_rss.mutex();
        });

    return *pmutex;
}

ServiceStatusE Manager::status(const Service& _rservice, std::unique_lock<ServiceMutexT>& _rlock) const
{
    const size_t   service_index = _rservice.index();
    ServiceStatusE status        = ServiceStatusE::Invalid;

    pimpl_->service_store_.aquire(
        service_index,
        [&_rlock, &status](const size_t _index, const ServiceStub& _rss) {
            _rlock = std::unique_lock<ServiceMutexT>{_rss.mutex()};
            status = _rss.status_;
        });

    return status;
}

ServiceStatusE Manager::status(const Service& _rservice)
{
    const size_t   service_index = _rservice.index();
    ServiceStatusE status        = ServiceStatusE::Invalid;

    pimpl_->service_store_.aquire(
        service_index,
        [&status](const size_t _index, ServiceStub& _rss) {
            status = _rss.status_.load(std::memory_order_relaxed);
        });

    return status;
}

size_t Manager::doForEachServiceActor(const Service& _rservice, const ActorVisitFunctionT& _rvisit_fnc)
{
    if (!_rservice.registered()) {
        return 0u;
    }
    const size_t                    service_index     = _rservice.index();
    size_t                          first_actor_chunk = InvalidIndex();
    std::unique_lock<ServiceMutexT> service_lock;
    ServiceStub&                    rss = pimpl_->service(service_index, service_lock);
    first_actor_chunk                   = rss.first_actor_chunk_;

    return doForEachServiceActor(first_actor_chunk, _rvisit_fnc);
}

size_t Manager::doForEachServiceActor(const size_t _first_actor_chunk, const ActorVisitFunctionT& _rvisit_fnc)
{

    size_t chunk_index   = _first_actor_chunk;
    size_t visited_count = 0;

    while (chunk_index != InvalidIndex()) {

        std::unique_lock<ChunkMutexT> chunk_lock;
        ActorChunkPtrT&               rchunk_ptr  = pimpl_->chunk(chunk_index * pimpl_->actor_chunk_size_, chunk_lock);
        ActorChunk&                   rchunk      = *rchunk_ptr;
        const size_t                  actor_count = rchunk.actor_count_;

        for (size_t i{0}, cnt{0}; i < pimpl_->actor_chunk_size_ && cnt < std::max(actor_count, rchunk.actor_count_); ++i) {
            ActorStub& ractor = rchunk[i];
            if (ractor.pactor_ != nullptr && ractor.reactor_ptr_) {
                VisitContext ctx(*this, ractor.reactor_ptr_, ractor.pactor_->runId());
                chunk_lock.unlock();
                _rvisit_fnc(ctx);
                ++visited_count;
                ++cnt;
                chunk_lock.lock();
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
        const size_t                  actor_index = static_cast<size_t>(_ractor.id());
        std::unique_lock<ChunkMutexT> chunk_lock;
        const ActorChunkPtrT&         rchunk_ptr = pimpl_->chunk(actor_index, chunk_lock);
        const ActorChunk&             rchunk     = *rchunk_ptr;

        service_index = rchunk.service_index_;
    }

    {
        std::unique_lock<ServiceMutexT> service_lock;
        const ServiceStub&              rss = pimpl_->service(service_index, service_lock);

        pservice = rss.pservice_;
    }
    return *pservice;
}

void Manager::doStartService(Service& _rservice, const OnLockedStartFunctionT& _on_locked_fnc)
{
    solid_check_log(_rservice.registered(), frame_logger, "Service not registered");

    {
        const size_t                    service_index = _rservice.index();
        std::unique_lock<ServiceMutexT> service_lock;
        ServiceStub&                    rss = pimpl_->service(service_index, service_lock);

        solid_check_log(rss.status_ == ServiceStatusE::Stopped, frame_logger, "Service not stopped");

        rss.status_ = ServiceStatusE::Running;

        try {
            _on_locked_fnc(service_lock);
        } catch (...) {
            {
                if (!service_lock) {
                    service_lock.lock(); // make sure we have the lock
                }
                rss.status_ = ServiceStatusE::Stopped;
            }
            throw;
        }
    }
}

void Manager::stopService(Service& _rservice, const bool _wait)
{
    solid_check_log(_rservice.registered(), frame_logger, "Service not registered");

    doStopService(_rservice.index(), _wait);
}

bool Manager::doStopService(const size_t _service_index, const bool _wait)
{
    std::unique_lock<ServiceMutexT> service_lock;
    ServiceStub*                    pss = pimpl_->servicePointer(_service_index, service_lock);

    if (pss == nullptr) {
        return false;
    }
    ServiceStub& rss = *pss;
    solid_log(frame_logger, Info, "" << _service_index << " actor_count = " << rss.actor_count_);

    if (rss.status_ == ServiceStatusE::Running) {
        rss.status_ = ServiceStatusE::Stopping;

        if (rss.pservice_) {
            rss.pservice_->onLockedStoppingBeforeActors();
        }

        const auto raise_lambda = [](VisitContext& _rctx) {
            return _rctx.wakeActor(generic_event<GenericEventE::Kill>);
        };
        const size_t cnt = doForEachServiceActor(rss.first_actor_chunk_, ActorVisitFunctionT{raise_lambda});

        if (cnt == 0 && rss.actor_count_ == 0) {
            solid_log(frame_logger, Verbose, "StateStoppedE on " << _service_index);
            rss.status_ = ServiceStatusE::Stopped;
            return true;
        }
    }

    if (rss.status_ == ServiceStatusE::Stopped) {
        solid_log(frame_logger, Verbose, "" << _service_index);
        return true;
    }

    if (rss.status_ == ServiceStatusE::Stopping && _wait) {
        while (rss.actor_count_ != 0u) {
            pimpl_->condition_.wait(service_lock);
        }
        rss.status_ = ServiceStatusE::Stopped;
    }
    return true;
}

void Manager::start()
{
    {
        std::unique_lock<std::mutex> lock(pimpl_->mutex_);

        while (pimpl_->status_ != StatusE::Running) {
            if (pimpl_->status_ == StatusE::Stopped) {
                pimpl_->status_ = StatusE::Running;
                continue;
            }
            if (pimpl_->status_ == StatusE::Running) {
                continue;
            }

            if (pimpl_->status_ == StatusE::Stopping) {
                while (pimpl_->status_ == StatusE::Stopping) {
                    pimpl_->condition_.wait(lock);
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
        std::unique_lock<std::mutex> lock(pimpl_->mutex_);
        if (pimpl_->status_ == StatusE::Stopped) {
            return;
        }
        if (pimpl_->status_ == StatusE::Running) {
            pimpl_->status_ = StatusE::Stopping;
        } else if (pimpl_->status_ == StatusE::Stopping) {
            while (pimpl_->service_count_ != 0u) {
                pimpl_->condition_.wait(lock);
            }
        } else {
            return;
        }
    }

    // broadcast to all actors to stop
    for (size_t service_index = 0; true; ++service_index) {
        if (!doStopService(service_index, false)) {
            break;
        }
    } // for

    // wait for all services to stop
    for (size_t service_index = 0; true; ++service_index) {
        std::unique_lock<ServiceMutexT> service_lock;
        ServiceStub*                    pss = pimpl_->servicePointer(service_index, service_lock);

        if (pss != nullptr) {

            if (pss->pservice_ != nullptr && pss->status_ == ServiceStatusE::Stopping) {
                solid_log(frame_logger, Verbose, "wait stop service: " << service_index);
                while (pss->actor_count_ != 0u) {
                    pimpl_->condition_.wait(service_lock);
                }
                pss->status_ = ServiceStatusE::Stopped;
                solid_log(frame_logger, Verbose, "StateStoppedE on " << service_index);
            }
        } else {
            break;
        }
    } // for

    {
        std::lock_guard<std::mutex> lock(pimpl_->mutex_);
        pimpl_->status_ = StatusE::Stopped;
        pimpl_->condition_.notify_all();
    }
}
//-----------------------------------------------------------------------------
bool Manager::VisitContext::wakeActor(EventBase const& _revent) const
{
    return reactor_ptr_->wake(actor_run_id_, _revent);
}

bool Manager::VisitContext::wakeActor(EventBase&& _uevent) const
{
    return reactor_ptr_->wake(actor_run_id_, std::move(_uevent));
}
//-----------------------------------------------------------------------------

} // namespace frame
} // namespace solid
