/* Declarations file signals.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TEST_CORE_SIGNALS_HPP
#define TEST_CORE_SIGNALS_HPP

#include "foundation/signal.hpp"
#include "object.hpp"



namespace test{


typedef Object::RequestUidTp	RequestUidTp;

//!	A signal for sending istreams from the fileManager
struct IStreamSignal: Dynamic<IStreamSignal, foundation::Signal>{
	IStreamSignal(StreamPtr<IStream> &_sptr, const FileUidTp &_rfuid, const RequestUidTp &_requid):sptr(_sptr), fileuid(_rfuid), requid(_requid){}
	int execute(uint32 _evs, foundation::SignalExecuter&, const SignalUidTp &, TimeSpec &);
	StreamPtr<IStream>	sptr;
	FileUidTp			fileuid;
	RequestUidTp		requid;
};

//!A signal for sending ostreams from the fileManager
struct OStreamSignal: Dynamic<OStreamSignal, foundation::Signal>{
	OStreamSignal(StreamPtr<OStream> &_sptr, const FileUidTp &_rfuid, const RequestUidTp &_requid):sptr(_sptr), fileuid(_rfuid), requid(_requid){}
	int execute(uint32 _evs, foundation::SignalExecuter&, const SignalUidTp &, TimeSpec &);
	StreamPtr<OStream>	sptr;
	FileUidTp			fileuid;
	RequestUidTp		requid;
};


//!A signal for sending iostreams from the fileManager
struct IOStreamSignal: Dynamic<IOStreamSignal, foundation::Signal>{
	IOStreamSignal(StreamPtr<IOStream> &_sptr, const FileUidTp &_rfuid, const RequestUidTp &_requid):sptr(_sptr), fileuid(_rfuid), requid(_requid){}
	int execute(uint32 _evs, foundation::SignalExecuter&, const SignalUidTp &, TimeSpec &);
	StreamPtr<IOStream>	sptr;
	FileUidTp			fileuid;
	RequestUidTp		requid;
};


//!A signal for sending errors from the fileManager
struct StreamErrorSignal: Dynamic<StreamErrorSignal, foundation::Signal>{
	StreamErrorSignal(int _errid, const RequestUidTp &_requid):errid(_errid), requid(_requid){}
	int execute(uint32 _evs, foundation::SignalExecuter&, const SignalUidTp &, TimeSpec &);
	int				errid;
	RequestUidTp	requid;
};


}//namespace test


#endif
