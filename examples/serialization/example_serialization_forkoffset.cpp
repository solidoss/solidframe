// forkoffset.cpp
//
// Copyright (c) 2012 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <iostream>
#include <stack>
#include <vector>
#include <deque>
#include <map>
#include <list>
#include <fstream>
#include <unistd.h>
#include "solid/serialization/binary.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstring>

using namespace std;
using namespace solid;


streampos stream_size(iostream &_rios){
	streampos pos = _rios.tellg();
	_rios.seekg(0, _rios.end);
	streampos endpos = _rios.tellg();
	_rios.seekg(pos);
	return endpos;
}

///\cond 0
struct Test{
	Test(const char *_fn = nullptr);
	template <class S>
	void serialize(S &_s){
		_s.push(no, "Test::no").template pushCall([this](S &_rs, uint64_t _val, ErrorConditionT &_rerr){return serializationReinit(_rs, _val, _rerr);}, 0, "Test::call").push(fn,"Test::fn");
	}
	
	template <class S>
	void serializationReinit(S &_rs, const uint64_t &_rv, ErrorConditionT &_rerr){
		idbg("_rv = "<<_rv);
		if(_rv == 0){
			if(S::IsSerializer){
				fs.open(fn.c_str());
				offset = stream_size(fs)/2;
				_rs.template pushCall([this](S &_rs, uint64_t _val, ErrorConditionT &_rerr){return serializationReinit(_rs, _val, _rerr);}, 1, "Test::reinit");
				_rs.push(offset, "offset");
			}else{
				_rs.template pushCall([this](S &_rs, uint64_t _val, ErrorConditionT &_rerr){return serializationReinit(_rs, _val, _rerr);}, 1, "Test::reinit");
				_rs.push(offset, "offset");
			}
		}else if(_rv == 1){
			if(S::IsSerializer){
				_rs.template pushCall([this](S &_rs, uint64_t _val, ErrorConditionT &_rerr){return serializationReinit(_rs, _val, _rerr);}, 2, "Test::reinit");
				idbg("put stream from "<<offset<<" len = "<<stream_size(fs)/2);
				istream *ps = &fs;
				_rs.pushStream(ps, offset, stream_size(fs)/2, "Test::istream");
			}else{
				fn += ".xxx";
				fs.open(fn.c_str(), fstream::out | fstream::binary);
				_rs.template pushCall([this](S &_rs, uint64_t _val, ErrorConditionT &_rerr){return serializationReinit(_rs, _val, _rerr);}, 2, "Test::reinit");
				ostream *ps = &fs;
				_rs.pushStream(ps, 0, 1000, "Test::ostream");
			}	
		}else{
			idbg("Done Stream: size = "<<_rs.streamSize()<<" error = "<<_rs.streamError().message());
		}
		
	}
	
	void print();
private:
	int32_t 	no;
	string		fn;
	uint64_t	offset;
	fstream		fs;
};
///\endcond

Test::Test(const char *_fn):fn(_fn?_fn:""){
	if(_fn) no = 1111;
}
//-----------------------------------------------------------------------------------

void Test::print(){
	cout<<endl<<"Test:"<<endl;
	cout<<"no = "<<no<<endl;
	cout<<"fn = "<<fn<<endl<<endl;
	cout<<"offset = "<<offset<<endl;
}

///\cond 0
void parentRun(int _sd, const char *_fn);
void childRun(int _sd);


typedef serialization::binary::Serializer<>			BinSerializer;
typedef serialization::binary::Deserializer<>		BinDeserializer;

///\endcond

int main(int argc, char *argv[]){
	int sps[2];
	if(argc != 2){
		cout<<"Expecting an existing file path as parameter!"<<endl;
		return 0;
	}

	int rv = socketpair(PF_UNIX, SOCK_STREAM, 0, sps);
	if(rv < 0){
		cout<<"error creating socketpair: "<<strerror(errno)<<endl;
		return 0;
	}

	rv = fork();
	if(rv){//the parent
#ifdef SOLID_HAS_DEBUG
		std::string dbgout;
		Debug::the().levelMask("view");
		Debug::the().moduleMask("ser_bin any");
		Debug::the().initFile("fork_parent", false, 3, 1024 * 1024 * 64, &dbgout);
		cout<<"debug log to: "<<dbgout<<endl;
#endif
		parentRun(sps[0], argv[1]);
	}else{//the child
#ifdef SOLID_HAS_DEBUG
		std::string dbgout;
		Debug::the().levelMask("view");
		Debug::the().moduleMask("ser_bin any");
		Debug::the().initFile("fork_child", false, 3, 1024 * 1024 * 64, &dbgout);
		cout<<"debug log to: "<<dbgout<<endl;
#endif
		childRun(sps[1]);
	}
	return 0;
}
enum {BUFSZ = 4 * 1024};
void parentRun(int _sd, const char *_fn){
	char buf[BUFSZ];
	Test t(_fn);
	BinSerializer	ser;
	ser.push(t, "test");
	t.print();
	int rv;
	int cnt = 0;
	while((rv = ser.run(buf, BUFSZ)) != 0){
		cnt += rv;
		if(write(_sd, buf, rv) != rv){
			cout<<"Error write: "<<strerror(errno)<<endl;
			return;
		}
	}
	cout<<"Parent finished, writted "<<cnt<<endl;
	sleep(1);
}
void childRun(int _sd){
	char buf[BUFSZ];
	Test t;
	BinDeserializer	des;
	des.push(t, "test");
	int rv;
	cout<<"Client reading"<<endl;
	while((rv = read(_sd, buf, BUFSZ)) > 0){
		cout<<"Client read: "<<rv<<endl;
		if(des.run(buf, rv) != rv) break;
		if(des.empty()) break;
	}
	if(rv < 0){
		cout<<"Error read: "<<strerror(errno)<<endl;
		return;
	}
	t.print();
}


