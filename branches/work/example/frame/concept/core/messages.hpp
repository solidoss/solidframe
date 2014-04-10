// messages.hpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef CONCEPT_CORE_MESSAGES_HPP
#define CONCEPT_CORE_MESSAGES_HPP

#include "frame/message.hpp"
#include "frame/file/filestore.hpp"
#include "common.hpp"

namespace concept{
//!	A signal for sending istreams from the fileManager
struct FilePointerMessage: solid::Dynamic<FilePointerMessage, solid::frame::Message>{
	FilePointerMessage():reqidx(0){}
	FilePointerMessage(
		solid::frame::file::FilePointerT _ptr,
		size_t _reqidx = 0, solid::uint32 _requid = 0
	):ptr(_ptr), reqidx(0), requid(_requid){}
	
	solid::frame::file::FilePointerT	ptr;
	size_t								reqidx;
	solid::uint32						requid;
};

}//namespace concept


#endif
