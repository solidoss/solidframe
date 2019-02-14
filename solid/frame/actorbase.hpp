// solid/frame/actorbase.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <vector>

#include "solid/frame/common.hpp"
#include "solid/utility/dynamicpointer.hpp"
#include "solid/utility/dynamictype.hpp"
#include <atomic>

namespace solid {
namespace frame {

class Manager;
class Service;
class ReactorBase;
class Actor;
class CompletionHandler;

class ActorBase : public Dynamic<ActorBase> {
public:
    //! Get the id of the actor
    IndexT id() const;

    //! Virtual destructor
    virtual ~ActorBase();

    UniqueId const& runId() const;

protected:
    friend class Service;
    friend class Manager;
    friend class ReactorBase;

    //! Constructor
    ActorBase();

    void unregister(Manager& _rm);
    bool isRegistered() const;

    virtual void onStop(Manager& _rm);

    bool disableVisits(Manager& _rm);

private:
    void id(const IndexT& _fullid);

    void runId(UniqueId const& _runid);
    void prepareSpecific();

    void stop(Manager& _rm);

private:
    std::atomic<IndexT> fullid;
    UniqueId            runid;
};

inline IndexT ActorBase::id() const
{
    return fullid.load(/*std::memory_order_relaxed*/);
}

inline bool ActorBase::isRegistered() const
{
    return fullid.load(/*std::memory_order_relaxed*/) != InvalidIndex();
}

inline void ActorBase::id(IndexT const& _fullid)
{
    fullid.store(_fullid /*, std::memory_order_relaxed*/);
}

inline UniqueId const& ActorBase::runId() const
{
    return runid;
}

inline void ActorBase::runId(UniqueId const& _runid)
{
    runid = _runid;
}

// inline size_t ActorBase::grabSignalMask(const size_t _leave /* = 0*/)
// {
//     return smask.fetch_and(_leave /*, std::memory_order_seq_cst*/);
// }
//
// inline bool ActorBase::notify(const size_t _smask)
// {
//     const size_t osm = smask.fetch_or(_smask /*, std::memory_order_seq_cst*/);
//     return (_smask | osm) != osm;
// }

} //namespace frame
} //namespace solid
