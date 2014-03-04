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

template <class S, class Cmd, class Pth, class Base>
struct OpenCommand: Base{
	Cmd				cmd;
	S				&rstore;
	Pth				inpath;
	
	
	OpenCommand(
		S &_rs, Cmd &_cmd, const char *_path, size_t _openflags
	):Base(_openflags), cmd(_cmd), rstore(_rs){}
	
	void prepare(FilePointerT &_runiptr, size_t &_ridx){
		_ridx = _runiptr.id().first;
		Base::prepareOpenFile(rstore.controller(), inpath, _ridx);
	}
	
	void operator()(shared::Context<File>	&_rctx){
		ERROR_NS::error_code	err;
		Base::openFile(rstore.controller(), *_rctx, err);
		if(err){
			_rctx.error(err);
		}
		cmd(_rctx);
	}
};

struct Utf8Controller;

struct CreateTempCommandBase{
	size_t			openflags;
	uint64			size;
	size_t			openidx;
protected:
	CreateTempCommandBase(
		uint64 _sz, size_t _openflags
	):openflags(_openflags), size(_sz), openidx(-1){}
	
	void prepareOpenTemp(Utf8Controller &_rctl, FilePointerT &_runiptr);
	void openTemp(Utf8Controller &_rctl, File &_rf, ERROR_NS::error_code &_rerr);
private:
};


template <class S, class Cmd>
struct CreateTempCommand: CreateTempCommandBase{
	S				&rstore;
	Cmd				cmd;
	
	CreateTempCommand(
		S &_rs, Cmd &_cmd, uint64 _sz, size_t _createflags
	):CreateTempCommandBase(_sz, _createflags), rstore(_rs), cmd(_cmd){}
	
	void prepare(FilePointerT &_runiptr, size_t &_ridx){
		this->prepareOpenTemp(rstore.controller(), _runiptr);
		if(!_runiptr.empty()){
			//the temp file can be open immediately
			_ridx = _runiptr.id().first;
		}
	}
	
	void operator()(shared::Context<File>	&_rctx){
		ERROR_NS::error_code	err;
		this->openTemp(rstore.controller(), *_rctx, err);
		if(err){
			_rctx.error(err);
		}
		cmd(_rctx);
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

struct Utf8Controller: public shared::Store<File, Utf8Controller>{
	typedef std::pair<uint8_t, std::string>	PathT;
	typedef Utf8OpenCommandBase				OpenCommandBaseT;
	
	Utf8Controller(const Utf8Configuration &_rcfg, const TempConfiguration &_rtmpcfg);
	~Utf8Controller();
private:
	friend struct Utf8OpenCommandBase;
	friend struct CreateTempCommandBase;
	
	void prepareOpenFile(const char *_inpath, PathT &_routpath, size_t &_ridx);
	void openFile(File &_rf, const PathT &_path, size_t _flags, ERROR_NS::error_code &_rerr);
	
	void prepareOpenTemp(FilePointerT &_uniptr, const uint64 _sz, size_t &_ropenidx);
	void openTemp(File &_rf, size_t _openidx, size_t _flags, ERROR_NS::error_code &_rerr);
private:
	struct Data;
	Data &d;
};

struct Utf8OpenCommandBase{
	Utf8Controller::PathT		outpath;
	size_t						openflags;
	
	Utf8OpenCommandBase(size_t _openflags):openflags(_openflags){}
	
	void prepareOpenFile(Utf8Controller &_rctl, const char *_path, size_t &_ridx);
	void openFile(Utf8Controller &_rctl, File &_rf, ERROR_NS::error_code &_rerr);
};

template <class Base = Utf8Controller>
class Store: public Base{
	
	typedef Base								BaseT;
	typedef Store<Base>							ThisT;

public:
	template <typename C>
	Store(C _c): BaseT(_c){}
	
	template <typename C1, typename C2>
	Store(C1 _c1, C2 _c2): BaseT(_c1, _c2){}
	
	//If a file with _path already exists in the store, the call will be similar with open with truncate openflag
	template <typename F, typename P>
	bool requestCreateFile(F _f, P _path, const size_t _openflags = 0, const size_t _flags = 0){
		OpenCommand<
			ThisT, F, P, 
			typename BaseT::OpenCommandBaseT
		>	cmd(*this, _f, _path, _openflags | FileDevice::CreateE | FileDevice::TruncateE);
		return BaseT::requestReinit(cmd, _flags);
	}
	
	template <typename F, typename P>
	bool requestOpenFile(F _f, P _path, const size_t _openflags = 0, const size_t _flags = 0){
		OpenCommand<
			ThisT, F, P,
			typename BaseT::OpenCommandBaseT
		>	cmd(*this, _f, _path, _openflags);
		return BaseT::requestReinit(cmd, _flags);
	}
	
	template <typename F>
	bool requestCreateTemp(F _f, uint64 _sz, const size_t _createflags = AllLevelsFlag, const size_t _flags = 0){
		CreateTempCommand<ThisT, F>	cmd(*this, _f, _sz, _createflags);
		return BaseT::requestReinit(cmd, _flags);
	}
};


inline void CreateTempCommandBase::prepareOpenTemp(Utf8Controller &_rctl, FilePointerT &_runiptr){
	_rctl.prepareOpenTemp(_runiptr, this->size, openidx);
}

inline void CreateTempCommandBase::openTemp(Utf8Controller &_rctl, File &_rf, ERROR_NS::error_code &_rerr){
	_rctl.openTemp(_rf, openidx, this->openflags, _rerr);
}

inline void Utf8OpenCommandBase::prepareOpenFile(Utf8Controller &_rctl, const char *_path, size_t &_ridx){
	_rctl.prepareOpenFile(_path, outpath, _ridx);
}

inline void Utf8OpenCommandBase::openFile(Utf8Controller &_rctl, File &_rf, ERROR_NS::error_code &_rerr){
	_rctl.openFile(_rf, outpath, openflags, _rerr);
}

}//namespace file
}//namespace frame
}//namespace solid


#endif
