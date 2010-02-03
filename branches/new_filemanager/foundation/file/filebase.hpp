#ifndef FILE_BASE_HPP
#define FILE_BASE_HPP

#include "foundation/file/filemanager.hpp"
#include "foundation/requestuid.hpp"

namespace foundation{
namespace file{

class File{
public:
	enum{
		Bad = -1,
		Ok = 0,
		No = 1,
		MustSingal = 2,
		MustWait = 3,
		Timeout = 1,
		
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
