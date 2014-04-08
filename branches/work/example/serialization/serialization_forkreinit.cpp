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
#include "system/debug.hpp"
#include "system/thread.hpp"
#include "serialization/binary.hpp"
#include "serialization/idtypemapper.hpp"
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
	void serialize(S &_s){
		_s.push(no, "Test::no").template pushReinit<Test, 0>(this, 0, "Test::reinit").push(fn,"Test::fn");
	}
	
	template <class S, uint32 I>
	serialization::binary::CbkReturnValueE serializationReinit(S &_rs, const uint64 &_rv){
		idbg("_rv = "<<_rv);
		if(_rv == 1){
			idbg("Done Stream: size = "<<_rs.streamSize()<<" error = "<<_rs.streamErrorString());
			return serialization::binary::Success;
		}
		if(S::IsSerializer){
			fs.open(fn.c_str());
			_rs.pop();
			_rs.template pushReinit<Test, 0>(this, 1, "Test::reinit");
			istream *ps = &fs;
			_rs.pushStream(ps, "Test::istream");
		}else{
			fn += ".xxx";
			fs.open(fn.c_str());
			_rs.pop();
			_rs.template pushReinit<Test, 0>(this, 1, "Test::reinit");
			ostream *ps = &fs;
			_rs.pushStream(ps, "Test::ostream");
		}
		return serialization::binary::Continue;
	}
	
	void print();
private:
	int32 		no;
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
typedef serialization::IdTypeMapper<
	BinSerializer,
	BinDeserializer,
	uint32
>													TypeMapper;

static TypeMapper		tpmap;

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
		Thread::init();
#ifdef UDEBUG
		std::string dbgout;
		Debug::the().levelMask("view");
		Debug::the().moduleMask("ser_bin any");
		Debug::the().initFile("fork_parent", false, 3, 1024 * 1024 * 64, &dbgout);
		cout<<"debug log to: "<<dbgout<<endl;
#endif
		parentRun(sps[0], argv[1]);
	}else{//the child
		Thread::init();
#ifdef UDEBUG
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
	BinSerializer	ser(tpmap);
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
	BinDeserializer	des(tpmap);
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


