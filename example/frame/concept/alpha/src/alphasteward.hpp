// alphasteward.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef ALPHASTEWARD_HPP
#define ALPHASTEWARD_HPP

#include "utility/dynamictype.hpp"
#include "frame/object.hpp"

namespace concept{

class Manager;
struct FilePointerMessage;

namespace alpha{
	
struct RemoteListMessage;
struct FetchMasterMessage;
struct FetchSlaveMessage;

//! A simple Steward object
class Steward: public solid::Dynamic<Steward, solid::frame::Object>{
public:
	static Steward& the(Steward *_ps = NULL);
	
	Steward(Manager &_rmgr);
	~Steward();
	
	void sendMessage(solid::DynamicPointer<solid::frame::Message> &_rmsgptr);
	
	void dynamicHandle(solid::DynamicPointer<> &_dp);
	void dynamicHandle(solid::DynamicPointer<RemoteListMessage> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<FetchMasterMessage> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<FetchSlaveMessage> &_rmsgptr);
	void dynamicHandle(solid::DynamicPointer<FilePointerMessage> &_rmsgptr);
	
private:
	void doExecute(solid::DynamicPointer<RemoteListMessage> &_rmsgptr);
	void doClearFetch(const size_t _idx);
	/*virtual*/ void execute(ExecuteContext &_rexectx);
	/*virtual*/ bool notify(solid::DynamicPointer<solid::frame::Message> &_rmsgptr);
private:
	struct Data;
	Data &d;
};

}//namespace alpha
}//namespace concept


#endif
