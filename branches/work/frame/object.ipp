// frame/object.ipp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifdef NINLINES
#define inline
#endif

inline void Object::ExecuteContext::reschedule(){
	retval = RescheduleRequest;
}
inline void Object::ExecuteContext::close(){
	retval = CloseRequest;
}
inline void Object::ExecuteContext::leave(Object &_robj, DynamicPointer<Object> &_robjptr){
	retval = LeaveRequest;
	_robj.leave(_robjptr);
}
inline void Object::ExecuteContext::wait(){
	retval = WaitRequest;
}
inline void Object::ExecuteContext::waitUntil(const TimeSpec &_rtm){
	retval = WaitUntilRequest;
	waittm = _rtm;
}
inline void Object::ExecuteContext::waitFor(const TimeSpec &_rtm){
	retval = WaitUntilRequest;
	waittm = rcrttm;
	waittm += _rtm;
}

inline IndexT Object::threadId()const{
	return thrid.load(/*ATOMIC_NS::memory_order_seq_cst*/);
}
inline void Object::threadId(const IndexT &_thrid){
	thrid.store(_thrid/*, ATOMIC_NS::memory_order_seq_cst*/);

}

inline size_t Object::grabSignalMask(size_t _leave){
// 	ulong sm = smask;
// 	smask = sm & _leave;
// 	return sm;
	return smask.fetch_and(_leave/*, ATOMIC_NS::memory_order_seq_cst*/);
}
inline bool Object::notified() const {
	return smask.load(/*ATOMIC_NS::memory_order_seq_cst*/) != 0;
}
inline bool Object::notified(size_t _s) const{
	return (smask.load(/*ATOMIC_NS::memory_order_seq_cst*/) & _s) != 0;
}

inline bool Object::notify(size_t _smask){
// 	ulong oldmask = smask;
// 	smask |= _smask;
// 	return (smask != oldmask) && signaled(S_RAISE);
	size_t osm = smask.fetch_or(_smask/*, ATOMIC_NS::memory_order_seq_cst*/);
	return (_smask & S_RAISE) && !(osm & S_RAISE); 
}

inline bool Object::isRegistered()const{
	return fullid != static_cast<IndexT>(-1);
}

inline void Object::id(const IndexT &_fullid){
	fullid = _fullid;
}

#ifdef NINLINES
#undef inline
#endif
