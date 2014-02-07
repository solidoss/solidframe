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

struct Configuration{
	struct Storage{
		std::string 	path;
		uint32			level;
		uint64			capacity;
	};
	typedef std::vector<Storage>	StorageVectorT;
	
	StorageVectorT	storagevec;
};

struct Configuration{
	struct Storage{
		std::string		localpath;
		std::string		pathprefix;
	};
	struct TempStorage{
		std::string 	path;
		uint32			level;
		uint64			capacity;
	};
	typedef std::vector<TempStorage>	TempStorageVectorT;
	typedef std::vector<Storage>		StorageVectorT;
	
	StorageVectorT		storagevec;
	TempStorageVectorT	storagevec;
	
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

class Store;

struct OpenCommandBase{
	typedef Pointer<File>	PointerT;
	Store 			&rs;
	size_t			openflags;
	const char 		*inpath;
	std::string		outpath;
protected:
	OpenCommandBase(Store &_rs, const char *_path, size_t _openflags):rs(_rs), inpath(_path), openflags(_openflags){}
	
private:
	void doInsertPath(size_t &_ridx);
};

template <class Cmd>
struct OpenCommand: OpenCommandBase{
	Cmd				cmd;
	
	OpenCommand(Store &_rs, Cmd &_cmd, const char *_path, size_t _openflags):OpenCommandBase(_rs, _path, _openflags), cmd(_cmd){}
	
	void prepare(PointerT &_runiptr, size_t &_ridx){
		_ridx = _runiptr.id().first;
		doInsertPath(inpath, outpath, _ridx);
	}
	
	void operator()(Context<File>	&_rctx){
		const bool rv = (*_rctx).open(path.c_str(), openflags);
		if(!rv){
			_rctx.error(error_code());
		}
		cmd(_rctx);
	}
};

struct CreateTempCommandBase{
	typedef Pointer<File>	PointerT;
	Store 			&rs;
	size_t			openflags;
	uint64			size;
	std::string		outpath;
protected:
	CreateTempCommandBase(Store &_rs, uint64 _sz, size_t _openflags):rs(_rs), size(_sz), openflags(_openflags){}
	
private:
};


template <class Cmd>
struct CreateTempCommand: OpenCommandBase{
	Cmd				cmd;
	
	CreateTempCommand(Store &_rs, Cmd &_cmd, uint64 _sz, size_t _createflags):OpenCommandBase(_rs, _sz, _createflags), cmd(_cmd){}
	
	void prepare(PointerT &_runiptr, size_t &_ridx){
	}
	
	void operator()(Context<File>	&_rctx){
	}
};

struct Controller{
	struct Data;
	Data &d;
	
	Controller(const Configuration &_rcfg);
	~Controller();
	
};

class Store: public shared::Store<File, Controller>{
	typedef shared::Store<File, Controller> SharedStoreT;
public:
	
	Store(Configuration const &_rcfg):SharedStoreT(_rcfg){}
	
	//If a file with _path already exists in the store, the call will be similar with open with truncate openflag
	template <typename F>
	bool requestCreate(F _f, const char* _path, const size_t _openflags = 0, const size_t _flags = 0){
		OpenCommand<F>	cmd(*this, _f, _path, _openflags/*TODO: add Truncate flags*/);
		return requestReinit(cmd, _flags);
	}
	
	template <typename F>
	bool requestOpen(F _f, const char* _path, const size_t _openflags = 0, const size_t _flags = 0){
		OpenCommand<F>	cmd(*this, _f, _path, _openflags);
		return requestReinit(cmd, _flags);
		
	}
	template <typename F>
	bool requestCreateTemp(F _f, uint64 _sz, const size_t _createflags = AllLevelsFlag, const size_t _flags = 0){
		CreateTempCommand<F>	cmd(*this, _f, _sz, _createflags);
		return requestReinit(cmd, _flags);
	}
private:
	friend struct OpenCommandBase;
	friend struct CreateTempCommandBase;
	void insertPath(const char *_inpath, std::string &_outpath, size_t &_ridx);
};

void OpenCommandBase::doInsertPath(size_t& _ridx){
	rs.insertPath(inpath, outpath, _ridx);
}

}//namespace file
}//namespace frame
}//namespace solid


#endif
