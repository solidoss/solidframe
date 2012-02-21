/* Declarations file filemanager.hpp
	
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

#ifndef FOUNDATION_FILE_FILE_MANAGER_HPP
#define FOUNDATION_FILE_FILE_MANAGER_HPP

#include "utility/streampointer.hpp"

#include "foundation/object.hpp"

#include "foundation/common.hpp"

class InputStream;
class OutputStream;
class InputOutputStream;

namespace foundation{

class RequestUid;

namespace file{

struct Key;
struct Mapper;
struct FileInputStream;
struct FileInputOutputStream;
struct FileOutputStream;
class File;
/*! An asynchronous manager for accessing files
	
	Accessing a file means getting a stream (either ISstream, OutputStream
	or InputOutputStream) to that file.
	
	The request is asynchronous, which means that you ask for the stream,
	and the manager either gives or sends it to you when:<br>
	
	> the file is open<br>
	> no other stream conflicts with the given stream.<br>
	
	The given stream for a certain file have not be in conflict:<br>
	
	> multiple istreams are allowed in the same time.<br>
	> only one ostream/iostream is allowed at a time.<br>
	> no istream is given while there are pending (i)ostreams<br>
	
	You must inherit Manager::Controller and give an instance 
	when creating a Manager. The Manager::Controller specifies
	the means of signalling a stream or an error to the 
	object requesting it.
	
	Also it has support for multiple types of files:<br>
	> basic disk files<br>
	> temporary files (gets deleted when the last attached stream is 
	deleted);<br>
	> temporary memory files (like the above files but they are not 
	stored on disk but on memory)<br>
	<br>
	This support is available through different types of Mappers and
	Keys.<br>
	
	After the last stream for a file gets deleted,	if there are no 
	pending files for open, the file can remain open for certain
	amount of time (see Mapper::getTimeout).
*/

class Manager: public Dynamic<Manager, Object>{
public:
	enum {
		Create = 1, //!< Try create if it doesnt exist
		Truncate = 2,
		Append = 4,
		OpenR = 8,
		OpenW = 16,
		OpenRW = 32,
		Forced = 64, //!< Force the opperation
		InputOutputStreamRequest = 128, 
		NoWait = 256, //!< Fail if the opperation cannot be completed synchronously
		ForcePending = 512,
	};
	//! A stub for limiting the manager interface
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
	//! A class for controlling manager initialization
	struct Controller{
		virtual ~Controller();
		virtual void init(const InitStub &_ris) = 0;
		virtual void sendStream(
			StreamPointer<InputStream> &_sptr,
			const FileUidT &_rfuid,
			const RequestUid& _rrequid
		) = 0;
		virtual void sendStream(
			StreamPointer<OutputStream> &_sptr,
			const FileUidT &_rfuid,
			const RequestUid& _rrequid
		) = 0;
		virtual void sendStream(
			StreamPointer<InputOutputStream> &_sptr,
			const FileUidT &_rfuid,
			const RequestUid& _rrequid
		) = 0;
		virtual void sendError(
			int _error,
			const RequestUid& _rrequid
		) = 0;
		
		virtual bool release() = 0;//return true for deletion
	};
	
	//! A stub limiting the manager's interface
	struct Stub{
		InputStream* createInputStream(IndexT _fileid);
		OutputStream* createOutputStream(IndexT _fileid);
		InputOutputStream* createInputOutputStream(IndexT _fileid);
		
		void pushFileTempExecQueue(const IndexT &_idx, uint16 _evs = 0);//called by mapper
		Mapper &mapper(ulong _id);
		Controller& controller();
		uint32	fileUid(IndexT _uid)const;
		void push(
			const StreamPointer<InputStream> &_rsp,
			const FileUidT &_rfuid,
			const RequestUid& _rrequid
		);
		void push(
			const StreamPointer<InputOutputStream> &_rsp,
			const FileUidT &_rfuid,
			const RequestUid& _rrequid
		);
		void push(
			const StreamPointer<OutputStream> &_rsp,
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
	static Manager& the();
	static Manager& the(const IndexT &_ridx);
	//! Constructor receiving a Controller
	/*!
		The ownership of the given Controller object
		is taken by the manager.
	*/
	Manager(Controller *_pc);
	//! Destructor
	~Manager();
	
	//! Get the size of a file identified by its name - the file will not be open
	int64 fileSize(const char *_fn)const;
	
	//! Get the size of a file identified by its key - the file will not be open
	int64 fileSize(const Key &_rk)const;
	
public://stream funtions
	int stream(
		StreamPointer<InputStream> &_sptr,
		const RequestUid &_rrequid,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<OutputStream> &_sptr,
		const RequestUid &_rrequid,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<InputOutputStream> &_sptr,
		const RequestUid &_rrequid,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	
	//using specific request
	int streamSpecific(
		StreamPointer<InputStream> &_sptr,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<OutputStream> &_sptr,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<InputOutputStream> &_sptr,
		const char *_fn = NULL,
		uint32 _flags = 0
	);

	
	//second type of stream request methods
	int stream(
		StreamPointer<InputStream> &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const char *_fn,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<OutputStream> &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const char *_fn,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<InputOutputStream> &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const char *_fn,
		uint32 _flags = 0
	);
	
	//using specific request
	int streamSpecific(
		StreamPointer<InputStream> &_sptr,
		FileUidT &_rfuid,
		const char *_fn,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<OutputStream> &_sptr,
		FileUidT &_rfuid,
		const char *_fn,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<InputOutputStream> &_sptr,
		FileUidT &_rfuid,
		const char *_fn,
		uint32 _flags = 0
	);

	//third type of stream request methods
	int stream(
		StreamPointer<InputStream> &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const Key &_rk,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<OutputStream> &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const Key &_rk,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<InputOutputStream> &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const Key &_rk,
		uint32 _flags = 0
	);
	
	//using specific request
	int streamSpecific(
		StreamPointer<InputStream> &_sptr,
		FileUidT &_rfuid,
		const Key &_rk,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<OutputStream> &_sptr,
		FileUidT &_rfuid,
		const Key &_rk,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<InputOutputStream> &_sptr,
		FileUidT &_rfuid,
		const Key &_rk,
		uint32 _flags = 0
	);
	
	//fourth type of stream request methods
	int stream(
		StreamPointer<InputStream> &_sptr,
		const FileUidT &_rfuid,
		const RequestUid &_rrequid,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<OutputStream> &_sptr,
		const FileUidT &_rfuid,
		const RequestUid &_rrequid,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<InputOutputStream> &_sptr,
		const FileUidT &_rfuid,
		const RequestUid &_rrequid,
		uint32 _flags = 0
	);
	
	//using specific request
	int streamSpecific(
		StreamPointer<InputStream> &_sptr,
		const FileUidT &_rfuid,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<OutputStream> &_sptr,
		const FileUidT &_rfuid,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<InputOutputStream> &_sptr,
		const FileUidT &_rfuid,
		uint32 _flags = 0
	);
	
	//fifth type of stream request methods
	int stream(
		StreamPointer<InputStream> &_sptr,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<OutputStream> &_sptr,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<InputOutputStream> &_sptr,
		const char *_fn = NULL,
		uint32 _flags = 0
	);
	
	int stream(
		StreamPointer<InputStream> &_sptr,
		const Key &_rk,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<OutputStream> &_sptr,
		const Key &_rk,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<InputOutputStream> &_sptr,
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
	
	friend struct FileInputStream;
	friend struct FileInputOutputStream;
	friend struct FileOutputStream;

	void releaseInputStream(IndexT _fileid);
	void releaseOutputStream(IndexT _fileid);
	void releaseInputOutputStream(IndexT _fileid);
	
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
	/*virtual*/ void init(Mutex *);
private:
	friend struct Stub;
	
	struct Data;
	Data	&d;
};

}//namespace file
}//namespace foundation

#endif

