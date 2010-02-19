/* Declarations file filemanager.hpp
	
	Copyright 2010 Valentin Palade 
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

#ifndef FDT_FILE_MANAGER_HPP
#define FDT_FILE_MANAGER_HPP

#include "utility/streampointer.hpp"

#include "foundation/object.hpp"

#include "foundation/common.hpp"

class IStream;
class OStream;
class IOStream;

namespace foundation{

class RequestUid;

namespace file{

struct Key;
struct Mapper;
struct FileIStream;
struct FileIOStream;
struct FileOStream;
class File;

class Manager: public Object{
public:
	enum {
		Create = 1, //!< Try create if it doesnt exist
		Truncate = 2,
		Append = 4,
		OpenR = 8,
		OpenW = 16,
		OpenRW = 32,
		Forced = 64, //!< Force the opperation
		IOStreamRequest = 128, 
		NoWait = 256, //!< Fail if the opperation cannot be completed synchronously
		ForcePending = 512,
	};
	
	struct InitStub{
		uint32 registerMapper(Mapper *_pm)const;
	private:
		InitStub();
		InitStub(const InitStub&);
		InitStub& operator=(const InitStub&);
		InitStub(Manager &_rm):rm(_rm){}
		friend class Manager;
		Manager		&rm;
	};
	
	struct Controller{
		virtual ~Controller();
		virtual void init(const InitStub &_ris) = 0;
		virtual void sendStream(
			StreamPointer<IStream> &_sptr,
			const FileUidT &_rfuid,
			const RequestUid& _rrequid
		) = 0;
		virtual void sendStream(
			StreamPointer<OStream> &_sptr,
			const FileUidT &_rfuid,
			const RequestUid& _rrequid
		) = 0;
		virtual void sendStream(
			StreamPointer<IOStream> &_sptr,
			const FileUidT &_rfuid,
			const RequestUid& _rrequid
		) = 0;
		virtual void sendError(
			int _error,
			const RequestUid& _rrequid
		) = 0;
		virtual void removeFileManager() = 0;
	};
	
	struct Stub{
		IStream* createIStream(IndexT _fileid);
		OStream* createOStream(IndexT _fileid);
		IOStream* createIOStream(IndexT _fileid);
		
		void pushFileTempExecQueue(const IndexT &_idx, uint16 _evs = 0);//called by mapper
		Mapper &mapper(ulong _id);
		Controller& controller();
		uint32	fileUid(IndexT _uid)const;
		void push(
			const StreamPointer<IStream> &_rsp,
			const FileUidT &_rfuid,
			const RequestUid& _rrequid
		);
		void push(
			const StreamPointer<IOStream> &_rsp,
			const FileUidT &_rfuid,
			const RequestUid& _rrequid
		);
		void push(
			const StreamPointer<OStream> &_rsp,
			const FileUidT &_rfuid,
			const RequestUid& _rrequid
		);
		void push(
			int _err,
			const RequestUid& _rrequid
		);
	private:
		Stub();
		//Stub(const Stub&);
		Stub& operator=(const Stub&);
		Stub(Manager &_rm):rm(_rm){}
		friend class Manager;
		Manager		&rm;
	};
	
public:
	Manager(Controller *_pc);
	~Manager();
	//! Get the size of a file identified by its name - the file will not be open
	int64 fileSize(const char *_fn)const;
	
	//! Get the size of a file identified by its key - the file will not be open
	int64 fileSize(const Key &_rk)const;
	void mutex(Mutex *_pmut);
public://stream funtions
	int stream(
		StreamPointer<IStream> &_sptr,
		const RequestUid &_rrequid,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<OStream> &_sptr,
		const RequestUid &_rrequid,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<IOStream> &_sptr,
		const RequestUid &_rrequid,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	
	//using specific request
	int streamSpecific(
		StreamPointer<IStream> &_sptr,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<OStream> &_sptr,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<IOStream> &_sptr,
		const char *_fn = NULL,
		uint32 _flags = 0
	);

	
	//second type of stream request methods
	int stream(
		StreamPointer<IStream> &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const char *_fn,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<OStream> &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const char *_fn,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<IOStream> &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const char *_fn,
		uint32 _flags = 0
	);
	
	//using specific request
	int streamSpecific(
		StreamPointer<IStream> &_sptr,
		FileUidT &_rfuid,
		const char *_fn,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<OStream> &_sptr,
		FileUidT &_rfuid,
		const char *_fn,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<IOStream> &_sptr,
		FileUidT &_rfuid,
		const char *_fn,
		uint32 _flags = 0
	);

	//third type of stream request methods
	int stream(
		StreamPointer<IStream> &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const Key &_rk,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<OStream> &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const Key &_rk,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<IOStream> &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const Key &_rk,
		uint32 _flags = 0
	);
	
	//using specific request
	int streamSpecific(
		StreamPointer<IStream> &_sptr,
		FileUidT &_rfuid,
		const Key &_rk,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<OStream> &_sptr,
		FileUidT &_rfuid,
		const Key &_rk,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<IOStream> &_sptr,
		FileUidT &_rfuid,
		const Key &_rk,
		uint32 _flags = 0
	);
	
	//fourth type of stream request methods
	int stream(
		StreamPointer<IStream> &_sptr,
		const FileUidT &_rfuid,
		const RequestUid &_rrequid,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<OStream> &_sptr,
		const FileUidT &_rfuid,
		const RequestUid &_rrequid,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<IOStream> &_sptr,
		const FileUidT &_rfuid,
		const RequestUid &_rrequid,
		uint32 _flags = 0
	);
	
	//using specific request
	int streamSpecific(
		StreamPointer<IStream> &_sptr,
		const FileUidT &_rfuid,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<OStream> &_sptr,
		const FileUidT &_rfuid,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<IOStream> &_sptr,
		const FileUidT &_rfuid,
		uint32 _flags = 0
	);
	
	//fifth type of stream request methods
	int stream(
		StreamPointer<IStream> &_sptr,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<OStream> &_sptr,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<IOStream> &_sptr,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	
	int stream(
		StreamPointer<IStream> &_sptr,
		const Key &_rk,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<OStream> &_sptr,
		const Key &_rk,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<IOStream> &_sptr,
		const Key &_rk,
		uint32 _flags = 0
	);
private:
	int execute(ulong _evs, TimeSpec &_rtout);
	template <typename StreamP>
	int doGetStream(
		StreamP &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const Key &_rk,
		uint32 _flags
	);
	
	friend struct FileIStream;
	friend struct FileIOStream;
	friend struct FileOStream;

	void releaseIStream(IndexT _fileid);
	void releaseOStream(IndexT _fileid);
	void releaseIOStream(IndexT _fileid);
	
	int fileWrite(
		IndexT _fileid,
		const char *_pb,
		uint32 _bl,
		const int64 &_off,
		uint32 _flags
	);
	int fileRead(
		IndexT _fileid,
		char *_pb,
		uint32 _bl,
		const int64 &_off,
		uint32 _flags
	);
	int64 fileSize(IndexT _fileid);
	void doExecuteMappers();
	void doExecuteFile(const IndexT &_idx, const TimeSpec &_rtout);
	void doDeleteFiles();
	void doPrepareStop();
	void doScanTimeout(const TimeSpec &_rtout);
	void doSendStreams();
private:
	friend struct Stub;
	
	struct Data;
	Data	&d;
};

}//namespace file
}//namespace foundation

#endif

