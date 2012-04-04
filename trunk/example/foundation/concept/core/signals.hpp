/* Declarations file signals.hpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TEST_CORE_SIGNALS_HPP
#define TEST_CORE_SIGNALS_HPP

#include "system/socketaddress.hpp"
#include "utility/streampointer.hpp"
#include "foundation/signal.hpp"
#include "common.hpp"

class InputStream;
class OutputStream;
class InputOutputStream;

namespace concept{


//!	A signal for sending istreams from the fileManager
struct InputStreamSignal: Dynamic<InputStreamSignal, foundation::Signal>{
	InputStreamSignal(StreamPointer<InputStream> &_sptr, const FileUidT &_rfuid, const foundation::RequestUidT &_requid);
	int execute(DynamicPointer<Signal> &_rthis_ptr, uint32 _evs, foundation::SignalExecuter&, const SignalUidT &, TimeSpec &);
	StreamPointer<InputStream>	sptr;
	FileUidT				fileuid;
	RequestUidT				requid;
};

//!A signal for sending ostreams from the fileManager
struct OutputStreamSignal: Dynamic<OutputStreamSignal, foundation::Signal>{
	OutputStreamSignal(StreamPointer<OutputStream> &_sptr, const FileUidT &_rfuid, const foundation::RequestUidT &_requid);
	int execute(DynamicPointer<Signal> &_rthis_ptr, uint32 _evs, foundation::SignalExecuter&, const SignalUidT &, TimeSpec &);
	StreamPointer<OutputStream>	sptr;
	FileUidT				fileuid;
	RequestUidT				requid;
};


//!A signal for sending iostreams from the fileManager
struct InputOutputStreamSignal: Dynamic<InputOutputStreamSignal, foundation::Signal>{
	InputOutputStreamSignal(StreamPointer<InputOutputStream> &_sptr, const FileUidT &_rfuid, const foundation::RequestUidT &_requid);
	int execute(DynamicPointer<Signal> &_rthis_ptr, uint32 _evs, foundation::SignalExecuter&, const SignalUidT &, TimeSpec &);
	StreamPointer<InputOutputStream>	sptr;
	FileUidT				fileuid;
	RequestUidT				requid;
};


//!A signal for sending errors from the fileManager
struct StreamErrorSignal: Dynamic<StreamErrorSignal, foundation::Signal>{
	StreamErrorSignal(int _errid, const RequestUidT &_requid);
	int execute(DynamicPointer<Signal> &_rthis_ptr, uint32 _evs, foundation::SignalExecuter&, const SignalUidT &, TimeSpec &);
	int				errid;
	RequestUidT		requid;
};

struct SocketAddressInfoSignal: Dynamic<SocketAddressInfoSignal, foundation::Signal>{
	SocketAddressInfoSignal(uint32 _id = 0):id(_id){}
	void init(
		const char *_node, 
		int _port, 
		int _flags,
		int _family = -1,
		int _type = -1,
		int _proto = -1
	);
	void init(
		const char *_node, 
		const char *_port, 
		int _flags,
		int _family = -1,
		int _type = -1,
		int _proto = -1
	);
	virtual void result(const ObjectUidT &_rv);
	uint32					id;
	SocketAddressInfo		addrinfo;
	std::string				node;
	std::string				service;
	
};

}//namespace concept


#endif
