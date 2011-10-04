/* Declarations file filebase.hpp
	
	Copyright 2010 Valentin Palade 
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

#ifndef FOUNDATION_FILE_FILE_BASE_HPP
#define FOUNDATION_FILE_FILE_BASE_HPP

#include "foundation/file/filemanager.hpp"
#include "foundation/requestuid.hpp"

#include "utility/queue.hpp"

namespace foundation{
namespace file{
//!The base class for all types of files - disk, temp, memory etc.
class File{
public:
	enum{
		Bad = -1,
		Ok = 0,
		No = 1,
		MustSignal = 2,
		MustWait = 3,
		Timeout = 1,
		MustDie = 2,
		RetryOpen = 4,
	};
	enum States{
		Running  = 1,
		Opening,
		Destroy,
		Destroyed
	};
	
	virtual ~File();
	virtual int read(
		char *_pb,
		uint32 _bl,
		const int64 &_off,
		uint32 _flags
	) = 0;
	virtual int write(
		const char *_pb,
		uint32 _bl,
		const int64 &_off,
		uint32 _flags
	) = 0;
	virtual int64 size()const = 0;
	
	virtual int open(const char *_path = NULL) = 0;
	virtual bool close(const char *_path = NULL) = 0;
	virtual int64 capacity()const = 0;
	
	int stream(
		Manager::Stub &_rs,
		StreamPointer<IStream> &_sptr,
		const RequestUid &_requid,
		uint32 _flags
	);
	
	int stream(
		Manager::Stub &_rs,
		StreamPointer<OStream> &_sptr,
		const RequestUid &_requid,
		uint32 _flags
	);
	
	int stream(
		Manager::Stub &_rs,
		StreamPointer<IOStream> &_sptr,
		const RequestUid &_requid,
		uint32 _flags
	);
	bool isOpened()const;
	bool isRegistered()const;
	const IndexT& id()const;
	void id(const IndexT &_id);
	const Key& key()const;
	bool decreaseOutCount();
	bool decreaseInCount();
	
	bool tryRevive();
	
	int execute(
		Manager::Stub &_rs,
		uint16 _evs,
		TimeSpec &_rts,
		Mutex	&_mtx
	);
protected:
	File(const Key &_rk);
	int doDestroy(Manager::Stub &_rs, int _err = -1);
	int doRequestOpen(Manager::Stub &_rs);
	void doCheckOpenMode(Manager::Stub &_rs);
protected:
	struct WaitData{
		WaitData(const RequestUid &_rrequid, uint32 _flags):requid(_rrequid), flags(_flags){}
		RequestUid	requid;
		uint32		flags;
	};
	typedef Queue<WaitData>		WaitQueueT;
	Key				&rk;
	IndexT			idx;
	uint32			ousecnt;
	uint32			iusecnt;
	uint8			openmode;
	uint8			openmoderequest;
	uint8			openmode_old;
	uint8			openmoderequest_old;
	uint16			state;
	WaitQueueT		iwq;
	WaitQueueT		owq;
};

}//namespace file
}//namespace foundation

#endif
