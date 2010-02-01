#ifndef FILE_BASE_HPP
#define FILE_BASE_HPP

#include "foundation/file/filemanager.hpp"

namespace foundation{
namespace file{

class File{
public:
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
	
	int stream(
		Manager::Stub &_rs,
		IStream &_sptr,
		const RequestUid &_requid,
		const FileKey &_rk,
		uint32 _flags
	);
	
	int stream(
		Manager::Stub &_rs,
		OStream &_sptr,
		const RequestUid &_requid,
		const FileKey &_rk,
		uint32 _flags
	);
	
	int stream(
		Manager::Stub &_rs,
		IOStream &_sptr,
		const RequestUid &_requid,
		const FileKey &_rk,
		uint32 _flags
	);
	bool isOpened()const;
	bool isRegistered()const;
	uint32 id();
	const Key& key()const;
	bool decreaseOutCount();
	bool decreaseInCount();
	
	int execute(
		Manager::Stub &_rs,
		uint16 _evs,
		TimeSpec &_rts,
		Mutex	&_mtx
	);
protected:
	File(const Key &_rk);
protected:
	struct WaitData{
		WaitData(const RequestUid &_rrequid, uint32 _flags):requid(_rrequid), flags(_flags){}
		RequestUid	requid;
		uint32		flags;
	};
	typedef Queue<WaitData>		WaitQueueTp;
	Key				&rk;
	uint32			ousecnt;
	uint32			iusecnt;
	uint32			msectout;
	uint16			flags;
	uint16			state;
	WaitQueueTp		iwq;
	WaitQueueTp		owq;
};

}//namespace file
}//namespace foundation

#endif
