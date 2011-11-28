/* Implementation file fork.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <stack>
#include <vector>
#include <deque>
#include <map>
#include <list>
#include "system/filedevice.hpp"
#include "utility/iostream.hpp"
#undef UDEBUG
#include "system/thread.hpp"
#include "algorithm/serialization/binary.hpp"
#include "algorithm/serialization/typemapper.hpp"
#include "algorithm/serialization/idtypemap.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <cerrno>
#include <cstring>

using namespace std;
///\cond 0
class FileIOStream: public IOStream{
public:
	FileIOStream();
	~FileIOStream();
	int openRead(const char *_fn);
	int openWrite(const char *_fn);
	int read(char *, uint32, uint32 _flags = 0);
	int write(const char *, uint32, uint32 _flags = 0);
	int64 seek(int64, SeekRef);
	int64 size()const;
	void close();
private:
	FileDevice	fd;
};
///\endcond
FileIOStream::FileIOStream(){}
FileIOStream::~FileIOStream(){}
int FileIOStream::openRead(const char *_fn){
	return fd.open(_fn, FileDevice::RO);
}
int FileIOStream::openWrite(const char *_fn){
	return fd.open(_fn, FileDevice::WO | FileDevice::TR | FileDevice::CR);
}
int FileIOStream::read(char *_pb, uint32 _bl, uint32 _flags){
	return fd.read(_pb, _bl);
}
int FileIOStream::write(const char *_pb, uint32 _bl, uint32 _flags){
	return fd.write(_pb, _bl);
}
int64 FileIOStream::seek(int64, SeekRef){
	return -1;
}
int64 FileIOStream::size()const{
	return fd.size();
}
void FileIOStream::close(){
	fd.close();
}

///\cond 0
struct Test{
	Test(const char *_fn = NULL);
	template <class S>
	S& operator&(S &_s){
		return _s.template pushStreammer<Test>(this, "Test::fs").push(no, "Test::no").push(fn,"Test::fn");
	}
	int createDeserializationStream(OStream *&_rps, int64 &_rsz, uint64 &_roff, int _id);
	void destroyDeserializationStream(OStream *_ps, int64 &_rsz, uint64 &_roff, int _id);
	int createSerializationStream(IStream *&_rps, int64 &_rsz, uint64 &_roff, int _id);
	void destroySerializationStream(IStream *_ps, int64 &_rsz, uint64 &_roff, int _id);
	void print();
private:
	int32 			no;
	string			fn;
	FileIOStream	fs;
};
///\endcond

Test::Test(const char *_fn):fn(_fn?_fn:""){}
//-----------------------------------------------------------------------------------
void Test::destroyDeserializationStream(
	OStream *_ps, int64 &_rsz, uint64 &_roff, int _id
){
	cout<<"Destroy deserialization <"<<_id<<"> sz "<<_rsz<<endl;
}
int Test::createDeserializationStream(
	OStream *&_rps, int64 &_rsz, uint64 &_roff, int _id
){
	if(_id) return NOK;
	cout<<"Create deserialization <"<<_id<<"> sz "<<_rsz<<endl;
	if(fn.empty() /*|| _rps.second < 0*/) return BAD;
	fn += ".xxx";
	cout<<"File name <"<<fn<<endl;
	if(fs.openWrite(fn.c_str())){	
		fn.clear();
		cout<<"failed open des file"<<endl;
	}else{
		_rps = static_cast<OStream*>(&fs);
		cout<<"success oppening des file"<<endl;
	}
	return OK;
}
void Test::destroySerializationStream(
	IStream *_ps, int64 &_rsz, uint64 &_roff, int _id
){
	cout<<"doing nothing as the stream will be destroyed when the command will be destroyed"<<endl;
	//fs.close();
}

int Test::createSerializationStream(
	IStream *&_rps, int64 &_rsz, uint64 &_roff, int _id
){
	if(_id) return NOK;
	cout<<"Create serialization <"<<_id<<"> sz "<<_rsz<<endl;;
	//The stream is already opened
	cout<<"File name "<<fn<<endl;
	if(fn.empty() || fs.openRead(fn.c_str())){
		return BAD;
	}
	_rps = static_cast<IStream*>(&fs);
	_rsz = fs.size();
	cout<<"serializing stream"<<endl;
	return OK;
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


typedef serialization::TypeMapper					TypeMapper;
//typedef serialization::NameTypeMap					NameTypeMap;
typedef serialization::IdTypeMap					IdTypeMap;
typedef serialization::bin::Serializer				BinSerializer;
typedef serialization::bin::Deserializer			BinDeserializer;
///\endcond

int main(int argc, char *argv[]){
	int sps[2];
	if(argc != 2){
		cout<<"Expecting a file name as parameter!"<<endl;
		return 0;
	}

	int rv = socketpair(PF_UNIX, SOCK_STREAM, 0, sps);
	if(rv < 0){
		cout<<"error creating socketpair: "<<strerror(errno)<<endl;
		return 0;
	}
	TypeMapper::registerMap<IdTypeMap>(new IdTypeMap);
	TypeMapper::registerSerializer<BinSerializer>();

	rv = fork();
	if(rv){//the parent
		Thread::init();
		childRun(sps[1]);
		//close(sps[1]);
	}else{//the child
		Thread::init();
		parentRun(sps[0], argv[1]);
		//close(sps[0]);
	}
	return 0;
}
enum {BUFSZ = 4 * 1024};
void parentRun(int _sd, const char *_fn){
	char buf[BUFSZ];
	Test t(_fn);
	BinSerializer	ser(IdTypeMap::the());
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
	BinDeserializer	des(IdTypeMap::the());
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


