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
		Base::insertPath(rstore.controller(), inpath, _ridx);
	}
	
	void operator()(shared::Context<File>	&_rctx){
		ERROR_NS::error_code	err;
		const bool rv =  Base::open(rstore.controller(), *_rctx, err);  //(*_rctx).open(outpath.c_str(), openflags);
		if(!rv){
			_rctx.error(err);
		}
		cmd(_rctx);
	}
};

struct CreateTempCommandBase{
	size_t			openflags;
	uint64			size;
	std::string		outpath;
protected:
	CreateTempCommandBase(
		uint64 _sz, size_t _openflags
	):openflags(_openflags), size(_sz){}
	
private:
};


template <class S, class Cmd>
struct CreateTempCommand: CreateTempCommandBase{
	S				&rs;
	Cmd				cmd;
	
	CreateTempCommand(
		S &_rs, Cmd &_cmd, uint64 _sz, size_t _createflags
	):CreateTempCommandBase(_sz, _createflags), rs(_rs), cmd(_cmd){}
	
	void prepare(FilePointerT &_runiptr, size_t &_ridx){
	}
	
	void operator()(shared::Context<File>	&_rctx){
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
	
	void insertPath(const char *_inpath, PathT &_routpath, size_t &_ridx);
	bool open(File &_rf, const PathT &_path, size_t _flags, ERROR_NS::error_code &_rerr);
private:
	struct Data;
	Data &d;
};

struct Utf8OpenCommandBase{
	Utf8Controller::PathT		outpath;
	size_t						openflags;
	
	Utf8OpenCommandBase(size_t _openflags):openflags(_openflags){}
	
	void insertPath(Utf8Controller &_rctl, const char *_path, size_t &_ridx){
		_rctl.insertPath(_path, outpath, _ridx);
	}
	bool open(Utf8Controller &_rctl, File &_rf, ERROR_NS::error_code &_rerr){
		return _rctl.open(_rf, outpath, openflags, _rerr);
	}
};

template <class Ctl = Utf8Controller>
class Store: public shared::Store<File, Ctl>{
	
	typedef Ctl									ControllerT;
	typedef shared::Store<File, ControllerT>	SharedStoreT;
	typedef Store<Ctl>							ThisT;

public:
	explicit Store(const ControllerT &_rctl): SharedStoreT(_rctl){}
	
	template <typename C>
	Store(C _c): SharedStoreT(_c){}
	
	template <typename C1, typename C2>
	Store(C1 _c1, C2 _c2): SharedStoreT(_c1, _c2){}
	
	//If a file with _path already exists in the store, the call will be similar with open with truncate openflag
	template <typename F, typename P>
	bool requestCreate(F _f, P _path, const size_t _openflags = 0, const size_t _flags = 0){
		OpenCommand<
			ThisT, F, P, 
			typename ControllerT::OpenCommandBaseT
		>	cmd(*this, _f, _path, _openflags | FileDevice::CreateE | FileDevice::TruncateE);
		return SharedStoreT::requestReinit(cmd, _flags);
	}
	
	template <typename F, typename P>
	bool requestOpen(F _f, P _path, const size_t _openflags = 0, const size_t _flags = 0){
		OpenCommand<
			ThisT, F, P,
			typename ControllerT::OpenCommandBaseT
		>	cmd(*this, _f, _path, _openflags);
		return SharedStoreT::requestReinit(cmd, _flags);
	}
	
	template <typename F>
	bool requestCreateTemp(F _f, uint64 _sz, const size_t _createflags = AllLevelsFlag, const size_t _flags = 0){
		CreateTempCommand<ThisT, F>	cmd(*this, _f, _sz, _createflags);
		return SharedStoreT::requestReinit(cmd, _flags);
	}
};

}//namespace file
}//namespace frame
}//namespace solid


#endif
