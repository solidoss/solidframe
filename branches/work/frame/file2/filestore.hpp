// frame/file/filestore.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_FILE_STORE_HPP
#define SOLID_FRAME_FILE_STORE_HPP

#include "frame/sharedstore.hpp"
#include "frame/file2/filebase.hpp"
#include "system/filedevice.hpp"

namespace solid{
namespace frame{
namespace file{


enum{
	MemoryLevelFlag = 1,
	VeryFastLevelFlag = 2,
	FastLevelFlag = 4,
	NormalLevelFlag = 8,
	SlowLevelFlag = 16,
	AllLevelsFlag = 1 + 2 + 4 + 8 + 16,
};

struct TempConfiguration{
	struct Storage{
		std::string 	path;
		uint32			level;
		uint64			capacity;
	};
	
	typedef std::vector<Storage>		StorageVectorT;
	
	StorageVectorT		storagevec;
};

struct File{
	void clear(){
		fd.close();
		if(pfb){
			pfb->close();
		}
		delete pfb;
		pfb = NULL;
	}
	bool open(const char *_path, const size_t _openflags){
		return fd.open(_path, _openflags);
	}
	
	int read(char *_pb, uint32 _bl, int64 _off){
		if(!pfb){
			return fd.read(_pb, _bl, _off);
		}else{
			return pfb->read(_pb, _bl, _off);
		}
	}
	int write(const char *_pb, uint32 _bl, int64 _off){
		return -1;
	}
	int64 size()const{
		if(!pfb){
			return fd.size();
		}else{
			return pfb->size();
		}
	}
	bool truncate(int64 _len = 0){
		if(!pfb){
			return fd.truncate(_len);
		}else{
			return pfb->truncate(_len);
		}
	}
private:
	FileDevice	fd;
	FileBase	*pfb;
};

typedef shared::Pointer<File>	FilePointerT;

template <class S, class Cmd, class Base>
struct OpenCommand: Base{
	Cmd				cmd;
		
	OpenCommand(
		Cmd &_cmd, typename Base::PathT _path, size_t _openflags
	):Base(_path, _openflags), cmd(_cmd){}
	

	void operator()(S &_rstore, FilePointerT &_rptr, ERROR_NS::error_code err){
		Base::openFile(_rstore, _rptr, err);
		cmd(_rstore, _rptr, err);
	}
};

struct Utf8Controller;

struct CreateTempCommandBase{
	size_t			openflags;
	uint64			size;
	size_t			openidx;
	
	bool prepareIndex(
		Utf8Controller &_rstore, size_t &_ridx, size_t &_rflags, ERROR_NS::error_code &_rerr
	);

	bool preparePointer(
		Utf8Controller &_rstore, FilePointerT &_runiptr, size_t &_rflags, ERROR_NS::error_code &_rerr
	);
protected:
	CreateTempCommandBase(
		uint64 _sz, size_t _openflags
	):openflags(_openflags), size(_sz), openidx(-1){}
private:
};


template <class S, class Cmd>
struct CreateTempCommand: CreateTempCommandBase{
	Cmd				cmd;
	
	CreateTempCommand(
		Cmd &_cmd, uint64 _sz, size_t _createflags
	):CreateTempCommandBase(_sz, _createflags), cmd(_cmd){}
	
	void operator()(S &_rstore, FilePointerT &_rptr, ERROR_NS::error_code err){
		cmd(_rstore, _rptr, err);
	}
};

struct Utf8Configuration{
	struct Storage{
		std::string		localpath;
		std::string		pathprefix;
	};
	typedef std::vector<Storage>		StorageVectorT;
	
	StorageVectorT		storagevec;
};

struct Utf8OpenCommandBase;

struct Utf8Controller{
	typedef std::pair<uint8_t, std::string>	PathT;
	typedef Utf8OpenCommandBase				OpenCommandBaseT;
	
	Utf8Controller(const Utf8Configuration &_rcfg, const TempConfiguration &_rtmpcfg);
	~Utf8Controller();
private:
	friend struct Utf8OpenCommandBase;
	friend struct CreateTempCommandBase;
	
	bool prepareFileIndex(
		Utf8OpenCommandBase &_rcmd, size_t &_ridx, size_t &_rflags, ERROR_NS::error_code &_rerr
	);
	
	void prepareFilePointer(
		Utf8OpenCommandBase &_rcmd, FilePointerT &_rptr, size_t &_rflags, ERROR_NS::error_code &_rerr
	);
	
	void prepareTempIndex(
		CreateTempCommandBase &_rcmd, size_t &_rflags, ERROR_NS::error_code &_rerr
	);
	bool prepareTempPointer(
		CreateTempCommandBase &_rcmd, FilePointerT &_rptr, size_t &_rflags, ERROR_NS::error_code &_rerr
	);
	void openFile(Utf8OpenCommandBase &_rcmd, FilePointerT &_rptr, ERROR_NS::error_code &_rerr);
private:
	struct Data;
	Data &d;
};

struct Utf8OpenCommandBase{
	typedef const char*			PathT;
	
	Utf8Controller::PathT		outpath;
	const char					*inpath;//not safe but fast
	size_t						openflags;
	
	Utf8OpenCommandBase(const char* _inpath, size_t _openflags):inpath(_inpath), openflags(_openflags){}
	
	bool prepareIndex(
		Utf8Controller &_rstore, size_t &_ridx, size_t &_rflags, ERROR_NS::error_code &_rerr
	);

	bool preparePointer(
		Utf8Controller &_rstore, FilePointerT &_runiptr, size_t &_rflags, ERROR_NS::error_code &_rerr
	);
	void openFile(Utf8Controller &_rstore, FilePointerT &_rptr, ERROR_NS::error_code &_rerr);
};

template <class Base = Utf8Controller>
class Store: public shared::Store<File, Store<Base> >, public Base{
	
	typedef Base								BaseT;
	typedef shared::Store<File, Store<Base> >	StoreT;
	typedef Store<Base>							ThisT;

public:
	template <typename C>
	Store(C _c): BaseT(_c){}
	
	template <typename C1, typename C2>
	Store(C1 _c1, C2 _c2): BaseT(_c1, _c2){}
	
	//If a file with _path already exists in the store, the call will be similar with open with truncate openflag
	template <typename Cmd, typename Pth>
	bool requestCreateFile(Cmd _cmd, Pth _path, const size_t _openflags = 0, const size_t _flags = 0){
		OpenCommand<
			ThisT, Cmd, 
			typename BaseT::OpenCommandBaseT
		>	cmd(_cmd, _path, _openflags | FileDevice::CreateE | FileDevice::TruncateE);
		return StoreT::requestReinit(cmd, _flags);
	}
	
	template <typename Cmd, typename Pth>
	bool requestOpenFile(Cmd _cmd, Pth _path, const size_t _openflags = 0, const size_t _flags = 0){
		OpenCommand<
			ThisT, Cmd,
			typename BaseT::OpenCommandBaseT
		>	cmd(_cmd, _path, _openflags);
		return StoreT::requestReinit(cmd, _flags);
	}
	
	template <typename Cmd>
	bool requestCreateTemp(
		Cmd _cmd, uint64 _sz, const size_t _createflags = AllLevelsFlag, const size_t _flags = 0
	){
		CreateTempCommand<ThisT, Cmd>	cmd(_cmd, _sz, _createflags);
		return StoreT::requestReinit(cmd, _flags);
	}
};

inline bool CreateTempCommandBase::prepareIndex(
	Utf8Controller &_rstore, size_t &_ridx, size_t &_rflags, ERROR_NS::error_code &_rerr
){
	_rstore.prepareTempIndex(*this, _rflags, _rerr);
	return false;//we dont have any stored index
}

inline bool CreateTempCommandBase::preparePointer(
	Utf8Controller &_rstore, FilePointerT &_runiptr, size_t &_rflags, ERROR_NS::error_code &_rerr
){
	return _rstore.prepareTempPointer(*this, _runiptr, _rflags, _rerr);
}

inline bool Utf8OpenCommandBase::prepareIndex(
	Utf8Controller &_rstore, size_t &_ridx, size_t &_rflags, ERROR_NS::error_code &_rerr
){
	return _rstore.prepareFileIndex(*this, _ridx, _rflags, _rerr);
}

inline bool Utf8OpenCommandBase::preparePointer(
	Utf8Controller &_rstore, FilePointerT &_runiptr, size_t &_rflags, ERROR_NS::error_code &_rerr
){
	_rstore.prepareFilePointer(*this, _runiptr, _rflags, _rerr);
	return true;//we don't store _runiptr for later use
}
inline void Utf8OpenCommandBase::openFile(Utf8Controller &_rstore, FilePointerT &_rptr, ERROR_NS::error_code &_rerr){
	_rstore.openFile(*this, _rptr, _rerr);
}

}//namespace file
}//namespace frame
}//namespace solid


#endif
