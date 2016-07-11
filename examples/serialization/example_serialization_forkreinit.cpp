// forkreinit.cpp
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
#include <unistd.h>
#include <fstream>
#include "solid/system/debug.hpp"
#include "solid/serialization/binary.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstring>

using namespace std;
using namespace solid;

///\cond 0
struct Test{
	Test(const char *_fn = NULL);
	template <class S>
	void serializationReinit(S &_rs, uint64_t _rv, ErrorConditionT &_rerr){
		idbg("_rv = "<<_rv);
		if(_rv == 1){
			idbg("Done Stream: size = "<<_rs.streamSize()<<" error = "<<_rs.streamError().message());
			return;
		}
		if(S::IsSerializer){
			idbg("open file: "<<fn);
			fs.open(fn.c_str());
			if(fs){
				idbg("success open");
			}else{
				idbg("fail open");
			}
			_rs.template pushCall([this](S &_rs, uint64_t _val, ErrorConditionT &_rerr){return serializationReinit(_rs, _val, _rerr);}, 1, "Test::reinit");
			istream *ps = &fs;
			_rs.pushStream(ps, "Test::istream");
		}else{
			fn += ".xxx";
			idbg("open file: "<<fn);
			fs.open(fn.c_str(), fstream::out | fstream::binary);
			if(fs){
				idbg("success open");
			}else{
				idbg("fail open");
			}
			
			_rs.template pushCall([this](S &_rs, uint64_t _val, ErrorConditionT &_rerr){return serializationReinit(_rs, _val, _rerr);}, 1, "Test::reinit");
			ostream *ps = &fs;
			_rs.pushStream(ps, "Test::ostream");
		}
	}
	
	template <class S>
	void serialize(S &_s){
		_s.push(no, "Test::no").template pushCall([this](S &_rs, uint64_t _val, ErrorConditionT &_rerr){return serializationReinit(_rs, _val, _rerr);}, 0, "Test::call").push(fn,"Test::fn");
	}
	
	
	void print();
private:
	int32_t 	no;
	string		fn;
	fstream		fs;
};
///\endcond

Test::Test(const char *_fn):fn(_fn?_fn:""){
	if(_fn) no = 11111;
}
//-----------------------------------------------------------------------------------


void Test::print(){
	cout<<endl<<"Test:"<<endl;
	cout<<"no = "<<no<<endl;
	cout<<"fn = "<<fn<<endl<<endl;
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


