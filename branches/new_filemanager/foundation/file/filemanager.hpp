#ifndef FDT_FILE_MANAGER_HPP
#define FDT_FILE_MANAGER_HPP

namespace foundation{
namespace file{

struct FileIStream;
struct FileIOStream;
struct FileOStream;

class Manager: public Object{
public:
	struct Controller{
		virtual ~Controller(){}
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
	
	struct InitStub{
		uint32 registerMapper(Mapper *_pm);
	private:
		InitStub();
		InitStub(const InitStub&);
		InitStub& operator=(const InitStub&);
		InitStub(Manager &_rm):rm(_rm){}
		friend Manager;
		Manager		&rm;
	};
	
	struct Stub{
		IStream* createIStream(IndexT _fileid);
		OStream* createOStream(IndexT _fileid);
		IOStream* createIOStream(IndexT _fileid);
		
		void pushFileTempExecQueue(File *_pf);//called by mapper
		Mapper &mapper(ulong _id);
	private:
		Stub();
		//Stub(const Stub&);
		Stub& operator=(const Stub&);
		Stub(Manager &_rm):rm(_rm){}
		friend Manager;
		Manager		&rm;
	};
	
	enum {
		Forced = 1, //!< Force the opperation
		Create = 2, //!< Try create if it doesnt exist
		IOStreamRequest = 4, 
		NoWait = 8, //!< Fail if the opperation cannot be completed synchronously
		ForcePending = 16,
	};
	
public:
	Manager(Controller *_pc);
	~Manager();
	//! Get the size of a file identified by its name - the file will not be open
	int64 fileSize(const char *_fn)const;
	
	//! Get the size of a file identified by its key - the file will not be open
	int64 fileSize(const Key &_rk)const;
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
		const FileKey &_rk,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<OStream> &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const FileKey &_rk,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<IOStream> &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const FileKey &_rk,
		uint32 _flags = 0
	);
	
	//using specific request
	int streamSpecific(
		StreamPointer<IStream> &_sptr,
		FileUidT &_rfuid,
		const FileKey &_rk,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<OStream> &_sptr,
		FileUidT &_rfuid,
		const FileKey &_rk,
		uint32 _flags = 0
	);
	int streamSpecific(
		StreamPointer<IOStream> &_sptr,
		FileUidT &_rfuid,
		const FileKey &_rk,
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
		const FileKey &_rk,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<OStream> &_sptr,
		const FileKey &_rk,
		uint32 _flags = 0
	);
	int stream(
		StreamPointer<IOStream> &_sptr,
		const FileKey &_rk,
		uint32 _flags = 0
	);
private:
	int execute(ulong _evs, TimeSpec &_rtout);
	template <typename StreamP>
	int doGetStream(
		StreamP &_sptr,
		FileUidT &_rfuid,
		const RequestUid &_rrequid,
		const FileKey &_rk,
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
	void doExecuteRegistered(File &_rf, const TimeSpec &_rtout);
	void doExecuteUnregistered(File &_rf, const TimeSpec &_rtout);
	void doDeleteFiles();
	void doRegisterFiles();
	void doScanTimeout(const TimeSpec &_rtout);
private:
	friend struct Stub;
	
	struct Data;
	Data	&d;
};

}//namespace file
}//namespace foundation
