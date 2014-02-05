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

struct Configuration{
	struct Storage{
		std::string		localpath;
		std::string		pathprefix;
	};
	
	typedef std::vector<Storage>	StorageVectorT;
	
	StorageVectorT		storagevec;
};

struct File: FileBase{
	void clear();
	bool open(const char *_path, const size_t _openflags);
	/*virtual*/ int read(char *_pb, uint32 _bl, int64 _off);
	/*virtual*/ int write(const char *_pb, uint32 _bl, int64 _off);
	int64 size()const;
	bool truncate(int64 _len = 0);
private:
	FileDevice	fd;
};

class Store;

template <class Cmd>
struct OpenCommand{
	Store 			&rs;
	Cmd				cmd;
	size_t			openflags;
	const char 		*inpath;
	std::string		outpath;
	
	OpenCommand(Store &_rs, Cmd &_cmd, const char *_path, size_t _openflags):rs(_rs), cmd(_cmd), inpath(_path), openflags(_openflags){}
	
	bool prepare(size_t* &_rpidx){
		return _rs.insertPath(_path, outpath, _rpidx);
	}
	
	void operator()(Context<File>	&_rctx){
		const bool rv = (*_rctx).open(path.c_str(), openflags);
		if(!rv){
			_rctx.error(error_code());
		}
		cmd(_rctx);
	}
};

class Store: public shared::Store<File>{
public:
	Store(Configuration const &_rcfg);
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
	
private:
	struct Data;
	Data	&d;
};

}//namespace file
}//namespace frame
}//namespace solid


#endif
