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

#include "system/socketaddress.hpp"
#include "utility/streampointer.hpp"
#include "foundation/signal.hpp"
#include "object.hpp"

class IStream;
class OStream;
class IOStream;

namespace concept{


typedef Object::RequestUidTp	RequestUidTp;

//!	A signal for sending istreams from the fileManager
struct IStreamSignal: Dynamic<IStreamSignal, foundation::Signal>{
	IStreamSignal(StreamPointer<IStream> &_sptr, const FileUidTp &_rfuid, const RequestUidTp &_requid);
	int execute(uint32 _evs, foundation::SignalExecuter&, const SignalUidTp &, TimeSpec &);
	StreamPointer<IStream>	sptr;
	FileUidTp				fileuid;
	RequestUidTp			requid;
};

//!A signal for sending ostreams from the fileManager
struct OStreamSignal: Dynamic<OStreamSignal, foundation::Signal>{
	OStreamSignal(StreamPointer<OStream> &_sptr, const FileUidTp &_rfuid, const RequestUidTp &_requid);
	int execute(uint32 _evs, foundation::SignalExecuter&, const SignalUidTp &, TimeSpec &);
	StreamPointer<OStream>	sptr;
	FileUidTp				fileuid;
	RequestUidTp			requid;
};


//!A signal for sending iostreams from the fileManager
struct IOStreamSignal: Dynamic<IOStreamSignal, foundation::Signal>{
	IOStreamSignal(StreamPointer<IOStream> &_sptr, const FileUidTp &_rfuid, const RequestUidTp &_requid);
	int execute(uint32 _evs, foundation::SignalExecuter&, const SignalUidTp &, TimeSpec &);
	StreamPointer<IOStream>	sptr;
	FileUidTp				fileuid;
	RequestUidTp			requid;
};


//!A signal for sending errors from the fileManager
struct StreamErrorSignal: Dynamic<StreamErrorSignal, foundation::Signal>{
	StreamErrorSignal(int _errid, const RequestUidTp &_requid);
	int execute(uint32 _evs, foundation::SignalExecuter&, const SignalUidTp &, TimeSpec &);
	int				errid;
	RequestUidTp	requid;
};

struct AddrInfoSignal: Dynamic<AddrInfoSignal, foundation::Signal>{
	AddrInfoSignal(uint32 _id = 0):id(_id){}
	void init(
		const char *_node, 
		int _port, 
		int _flags,
		int _family = -1,
		int _type = -1,
		int _proto = -1
	);
	uint32			id;
	AddrInfo		addrinfo;
	std::string		node;
	std::string		service;
};

}//namespace concept


#endif
