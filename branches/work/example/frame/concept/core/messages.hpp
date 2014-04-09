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
#include "frame/file2/filestore.hpp"
#include "common.hpp"

namespace concept{
//!	A signal for sending istreams from the fileManager
struct FilePointerMessage: solid::Dynamic<FilePointerMessage, solid::frame::Message>{
	FilePointerMessage():reqid(0){}
	FilePointerMessage(solid::frame::file::FilePointerT _ptr, solid::uint32 _reqid = 0):ptr(_ptr), reqid(0){}
	
	solid::frame::file::FilePointerT	ptr;
	solid::uint32						reqid;
};

}//namespace concept


#endif
