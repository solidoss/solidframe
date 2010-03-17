/* Declarations file signalexecuter.hpp
	
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

#ifndef CS_SIGNAL_EXECUTER_HPP
#define CS_SIGNAL_EXECUTER_HPP

#include "foundation/object.hpp"
#include "foundation/common.hpp"

#include "utility/streampointer.hpp"
#include "utility/dynamicpointer.hpp"

#include <string>

class IStream;
class OStream;
class IOStream;

struct TimeSpec;

namespace foundation{
namespace ipc{
struct ConnectionUid;
}
//! An object which will only execute the received signals
/*!
	<b>Overview:</b><br>
	- received signals are given an unique id (used for example
		with file manager as request id).
	- execute the signals eventually repeatedly until the signals want to
		be destroyed or want to leave the executer.
	- also can execute a signal when receiving something for that signal
		(a stream from FileManager, another signal, a string etc)
	- The _requid parameter of the receiveSomething methods, will uniquely
		identify the signals and must be the same with SignalUidT parameter
		of the Signal::execute(SignalExecuter&, const SignalUidT &, TimeSpec &_rts).
	- The receiveSomething methods are forwards to the actual signals identified by
		_requid parameter.
	
	<b>Usage:</b><br>
	- Inherit from SignalExecuter and implement removeFromManager
		in which you should call Manager::the().removeObject(this);
	- In your manager, create some signalexecuters and register them
		using foundation::Manager::insertObject
	- Implement for your signals execute(SignalExecuter&, const SignalUidT &, TimeSpec &_rts);
	
	\see test/foundation/src/server.cpp test/foundation/alpha/src/alphasignals.cpp
	\see test::SignalExecuter test::alpha::FetchMasterSignal
	\see foundation::Signal foundation::Object
*/
class SignalExecuter: public Object{
public:
	SignalExecuter();
	~SignalExecuter();
	int signal(DynamicPointer<Signal> &_rsig);
	int execute(ulong _evs, TimeSpec &_rtout);
	virtual void removeFromManager() = 0;
	void mutex(Mutex *_pmut);
	void sendSignal(
		DynamicPointer<Signal> &_rsig,
		const RequestUidT &_requid,
		const ObjectUidT& _from = ObjectUidT(),
		const ipc::ConnectionUid *_conid = NULL
	);
private:
	int execute();
	void doExecute(uint _pos, uint32 _evs, const TimeSpec &_rtout);
	struct Data;
	Data	&d;
};

}//namespace foundation

#endif
