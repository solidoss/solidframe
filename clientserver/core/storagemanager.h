/* Declarations file storagemanager.h
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef STORAGE_MANAGER_H
#define STORAGE_MANAGER_H

#include "utility/streamptr.h"

#include "clientserver/core/object.h"

#include "common.h"

class IStream;
class OStream;
class IOStream;

class Mutex;

namespace clientserver{
class File;

class StorageManager: public clientserver::Object{
public:
	StorageManager(uint32 _maxfcnt = 256);
	~StorageManager();
	int execute(ulong _evs, TimeSpec &_rtout);
	int istream(StreamPtr<IStream> &_sptr, const char *_fn, uint32 _idx, uint32 _uid);
	int ostream(StreamPtr<OStream> &_sptr, const char *_fn, uint32 _idx, uint32 _uid);
	int iostream(StreamPtr<IOStream> &_sptr, const char *_fn);
	//overload from object
	void mutex(Mutex *_pmut);
	void release(File &_rf);
	int doUseFreeQueue(File* &_rpf, const char *_fn);
protected:
	virtual void sendStream(StreamPtr<IStream> &_sptr, uint32 _idx, uint32 _uid) = 0;
	virtual void sendStream(StreamPtr<OStream> &_sptr, uint32 _idx, uint32 _uid) = 0;
	virtual void sendStream(StreamPtr<IOStream> &_sptr, uint32 _idx, uint32 _uid) = 0;
	virtual void sendError(uint32 _idx, uint32 _uid);
private:
	int execute();
	struct Data;
	Data	&d;
};

}//namespace clientserver
#endif
