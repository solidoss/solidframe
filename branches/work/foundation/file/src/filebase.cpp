/* Implementation file filebase.cpp
	
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


#include "utility/iostream.hpp"

#include "foundation/file/filebase.hpp"
#include "foundation/file/filemapper.hpp"
#include "foundation/file/filekey.hpp"


#include "system/mutex.hpp"
#include "system/directory.hpp"

#include <cerrno>


namespace fdt=foundation;

namespace foundation{
namespace file{

//==================================================================
//	
//==================================================================
File::File(
	const Key &_rk
):	rk(*_rk.clone()), idx(-1), ousecnt(0), iusecnt(0),
	openmode(0), openmoderequest(0), state(Running){
}
/*virtual*/ File::~File(){
	if(rk.release()){
		delete &rk;
	}
}

int File::stream(
	Manager::Stub &_rs,
	StreamPointer<IStream> &_sptr,
	const RequestUid &_requid,
	uint32 _flags
){
	if(_flags & Manager::Forced){
		//if not open for reading or incompatible request for pending return bad
		if(!(openmode & Manager::OpenR) || (_flags & Manager::ForcePending)) return BAD;
	}else if(openmode & Manager::OpenR){
		if(_flags & Manager::ForcePending){
			iwq.push(WaitData(_requid, _flags));
			return (ousecnt || owq.size()) ? MustWait : MustSignal;
		}else if(ousecnt || owq.size()){
			iwq.push(WaitData(_requid, _flags));
			return MustWait;
		}
	}else{// prepare for reading
		openmoderequest |= (_flags | Manager::OpenR);
		iwq.push(WaitData(_requid, _flags));
		return (ousecnt || owq.size()) ? MustWait : MustSignal;
	}
	++iusecnt;
	_sptr = _rs.createIStream(this->id());
	return OK;
}

int File::stream(
	Manager::Stub &_rs,
	StreamPointer<OStream> &_sptr,
	const RequestUid &_requid,
	uint32 _flags
){
	if(_flags & Manager::Forced){
		//if not open for reading or incompatible request for pending return bad
		if(!(openmode & Manager::OpenW) || (_flags & Manager::ForcePending)) return BAD;
	}else if(openmode & Manager::OpenW){
		if(_flags & Manager::ForcePending){
			owq.push(WaitData(_requid, _flags));
			return (ousecnt || iusecnt || (owq.size() != 1)) ? MustWait : MustSignal;
		}else if(ousecnt || iusecnt || owq.size()){
			owq.push(WaitData(_requid, _flags));
			return MustWait;
		}
	}else{// prepare for reading
		openmoderequest |= (_flags | Manager::OpenW);
		owq.push(WaitData(_requid, _flags));
		return (ousecnt || iusecnt || (owq.size() != 1)) ? MustWait : MustSignal;
	}
	++ousecnt;
	_sptr = _rs.createOStream(this->id());
	return OK;
}

int File::stream(
	Manager::Stub &_rs,
	StreamPointer<IOStream> &_sptr,
	const RequestUid &_requid,
	uint32 _flags
){
	if(_flags & Manager::Forced){
		//if not open for reading or incompatible request for pending return bad
		if(!(openmode & Manager::OpenRW) || (_flags & Manager::ForcePending)) return BAD;
	}else if(openmode & Manager::OpenRW){
		if(_flags & Manager::ForcePending){
			owq.push(WaitData(_requid, _flags));
			return (ousecnt || iusecnt || (owq.size() != 1)) ? MustWait : MustSignal;
		}else if(ousecnt || iusecnt || owq.size()){
			owq.push(WaitData(_requid, _flags));
			return MustWait;
		}
	}else{// prepare for reading
		openmoderequest |= (_flags | Manager::OpenRW);
		owq.push(WaitData(_requid, _flags | Manager::IOStreamRequest));
		return (ousecnt || iusecnt || (owq.size() != 1)) ? MustWait : MustSignal;
	}
	++ousecnt;
	_sptr = _rs.createIOStream(this->id());
	return OK;
}
bool File::isOpened()const{
	return openmode != 0;
}
bool File::isRegistered()const{
	return idx != (IndexT)-1;
}
const IndexT& File::id()const{
	return idx;
}
void File::id(const IndexT &_id){
	idx = _id;
}
const Key& File::key()const{
	return rk;
}
bool File::decreaseOutCount(){
	--ousecnt;
	if(!ousecnt){
		//return true if there are waiting readers or writers or noone is waiting
		return iwq.size() || owq.size() || (owq.empty() && iwq.empty());
	}
	//we must signal the filemanager to take the next writer
	return true;
}
bool File::decreaseInCount(){
	--iusecnt;
	if(!iusecnt){
		//return true if there are waiting writers or noone waiting
		return owq.size() || (owq.empty() && iwq.empty());
	}
	return false;
}

bool File::tryRevive(){
	if(state != Destroyed && (iwq.size() || owq.size())){
		state = Running;
		return true;
	}
	return false;//go ahead delete me!!!
}

int File::execute(
	Manager::Stub &_rs,
	uint16 _evs,
	TimeSpec &_rts,
	Mutex	&_mtx
){
	//NOTE: do not use state outside file::Manager's thread
	Mutex::Locker lock(_mtx);
	
	if(_evs & Timeout){
		if(iusecnt || ousecnt || iwq.size() || owq.size()){
			state = Running;	
		}else{
			state = Destroy;
		}
	}
	if(_evs & MustDie){
		state = Destroy;
	}
	if(ousecnt || iusecnt)
		return No;
	
	switch(state){
		case Destroy:
			//signall error for all requests
			return doDestroy(_rs);
		case Opening:
			if(!(_evs & RetryOpen)) return No;
			return doRequestOpen(_rs);
		default:break;
	}
	if(owq.size()){//
		if(owq.front().flags & Manager::IOStreamRequest){
			if(openmode & Manager::OpenRW){
				++ousecnt;
				//send iostream
				StreamPointer<IOStream> sptr(_rs.createIOStream(id()));	
				_rs.push(sptr, FileUidT(id(), _rs.fileUid(id())), owq.front().requid);
			}else{
				//(re)open request
				return doRequestOpen(_rs);
			}
		}else{
			if(openmode & Manager::OpenW){
				//send ostream
				++ousecnt;
				StreamPointer<OStream> sptr(_rs.createOStream(id()));	
				_rs.push(sptr, FileUidT(id(), _rs.fileUid(id())), owq.front().requid);
			}else{
				//(re)open request
				return doRequestOpen(_rs);
			}
		}
		owq.pop();
		return No;
	}
	if(iwq.size()){
		if(!(openmode & Manager::OpenR))
			return doRequestOpen(_rs);
		while(iwq.size()){
			++iusecnt;
			StreamPointer<IStream> sptr(_rs.createIStream(id()));	
			_rs.push(sptr, FileUidT(id(), _rs.fileUid(id())), iwq.front().requid);
			iwq.pop();
		}
		return No;
	}
	Mapper &rm(_rs.mapper(key().mapperId()));
	if(rm.getTimeout(_rts)) return No;
	state = Destroy;
	return Ok;
}

int File::doRequestOpen(Manager::Stub &_rs){
	Mapper &rm(_rs.mapper(key().mapperId()));
	
	//will call File::open(path);
	//which will use/set openmode and openmoderequest
	switch(rm.open(*this)){
		case File::Bad:
			state = Destroyed;
			doDestroy(_rs, errno);
			return Ok;
		case File::Ok:
			doCheckOpenMode(_rs);
			openmoderequest = 0;
			if(openmode & Manager::OpenRW){
				openmode |= (Manager::OpenW | Manager::OpenR);
			}
			state = Running;
			return Ok;
		case File::No:
			state = Opening;
			return No;
		default:
			cassert(false);
			break;
	}
	return Bad;
}


int File::doDestroy(Manager::Stub &_rs, int _err){
	//first signal all waiting peers the error
	while(iwq.size()){
		_rs.push(_err, iwq.front().requid);
		iwq.pop();
	}
	while(owq.size()){
		_rs.push(_err, owq.front().requid);
		owq.pop();
	}
	//close the file
	//this->close();
	Mapper &rm(_rs.mapper(key().mapperId()));
	rm.close(*this);
	return Bad;
}

void File::doCheckOpenMode(Manager::Stub &_rs){
	if(owq.size() && !(openmode & Manager::OpenW)){
		while(owq.size()){
			_rs.push(-1, owq.front().requid);
			owq.pop();
		}
	}
	if(iwq.size() && !(openmode & Manager::OpenR)){
		while(iwq.size()){
			_rs.push(-1, iwq.front().requid);
			iwq.pop();
		}
	}
	if(owq.size() && !(openmode & Manager::OpenRW)){
		uint32 owsz = owq.size();
		while(owsz--){
			WaitData wd(owq.front());
			owq.pop();
			if(wd.flags & Manager::IOStreamRequest){
				_rs.push(-1, wd.requid);
			}else{
				owq.push(wd);
			}
		}
	}
}

}//namespace file
}//namespace foundation


