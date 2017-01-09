// solid/frame/objectbase.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_OBJECTBASE_HPP
#define SOLID_FRAME_OBJECTBASE_HPP

#include <vector>

#include "solid/frame/common.hpp"
#include "solid/utility/dynamictype.hpp"
#include "solid/utility/dynamicpointer.hpp"
#include <atomic>

namespace solid{
namespace frame{

class Manager;
class Service;
class ReactorBase;
class Object;
class CompletionHandler;

class ObjectBase: public Dynamic<ObjectBase>{
public:
    //! Get the id of the object
    IndexT id() const;

    //! Virtual destructor
    virtual ~ObjectBase();

    UniqueId const& runId()const;

    bool isAcceptingEvents()const;

protected:
    friend class Service;
    friend class Manager;
    friend class ReactorBase;

    //! Constructor
    ObjectBase();

    //! Grab the signal mask eventually leaving some bits set- CALL this inside lock!!
    size_t grabSignalMask(const size_t _leave = 0);

    void unregister(Manager &_rm);
    bool isRegistered()const;
    virtual void doStop(Manager &_rm);

    bool notify(const size_t _smask);

    bool disableVisits(Manager &_rm);
private:
    void id(const IndexT &_fullid);

    void runId(UniqueId const& _runid);
    void prepareSpecific();

    void stop(Manager &_rm);
private:
    std::atomic<IndexT> fullid;
    UniqueId            runid;
    std::atomic<size_t> smask;
};

inline IndexT ObjectBase::id()  const {
    return fullid;
}

inline bool ObjectBase::isRegistered()const{
    return fullid.load() != InvalidIndex();
}

inline void ObjectBase::id(IndexT const &_fullid){
    fullid = _fullid;
}

inline UniqueId const& ObjectBase::runId()const{
    return runid;
}

inline void ObjectBase::runId(UniqueId const& _runid){
    runid = _runid;
}

inline size_t ObjectBase::grabSignalMask(const size_t _leave/* = 0*/){
    return smask.fetch_and(_leave/*, std::memory_order_seq_cst*/);
}

inline bool ObjectBase::notify(const size_t _smask){
    const size_t osm = smask.fetch_or(_smask/*, std::memory_order_seq_cst*/);
    return (_smask | osm) != osm;
}

}//namespace frame
}//namespace solid


#endif
