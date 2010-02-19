/* Implementation file open.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <iostream>
#include <fstream>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/exception.hpp>
#include <boost/utility.hpp>
#include "utility/workpool.hpp"
#include "system/filedevice.hpp"
#include <deque>
#include <cstring>
#include <cerrno>
using namespace std;

template <class T>
static T align(T _v, ulong _by);

template <class T>
static T* align(T* _v, const ulong _by){
    if((size_t)_v % _by){
        return _v + (_by - ((size_t)_v % _by));
    }else{
        return _v;
    }
}

template <class T>
static T align(T _v, const ulong _by){
    if(_v % _by){
        return _v + (_by - (_v % _by));
    }else{
        return _v;
    }
}

const uint32  pagesize = getpagesize();

///\cond 0
typedef std::deque<FileDevice>	FileDeuqeT;
typedef std::deque<auto_ptr<FileDevice> >	AutoFileDequeT;
///\endcond

///\cond 0
class MyWorkPool: public WorkPool<FileDevice*>{
public:
	void run(Worker &);
	void prepareWorker(){}
	void unprepareWorker(){}
protected:
	int createWorkers(uint);
};
///\endcond
void MyWorkPool::run(Worker &_wk){
	FileDevice* pfile;
	const ulong readsz = 4* pagesize;
	char *bf(new char[readsz + pagesize]) ;
	char *buf(align(bf, pagesize));
	//char buf[readsz];
	//cassert(buf == bf);
	while(pop(_wk.wid(), pfile) != BAD){
		idbg(_wk.wid()<<" is processing");
		int64 sz = pfile->size();
		int toread;
		int cnt = 0;
		while(sz > 0){
			toread = readsz;
			if(toread > sz) toread = sz;
			int rv = pfile->read(buf, toread);
			cnt += rv;
			sz -= rv;
		}
		//cout<<"read count "<<cnt<<endl;
		//Thread::sleep(100);
	}
	delete []bf;
}

int MyWorkPool::createWorkers(uint _cnt){
	for(uint i = 0; i < _cnt; ++i){
		Worker *pw = createWorker<Worker>(*this);
		pw->start(true);
	}
	return _cnt;
}


int main(int argc, char *argv[]){
	Thread::init();
	if(argc != 4){
		cout<<"./file_open_pool /path/to/folder file-count folder-count"<<endl;
		return 0;
	}
	char c;
	char name[1024];
	int filecnt   = atoi(argv[2]);
	int foldercnt   = atoi(argv[3]);
	sprintf(name, "%s", argv[1]);
	char *fldname = name + strlen(argv[1]);
	char *fname = name + strlen(argv[1]) + 1 + 8;
	FileDeuqeT	fdq;
	int cnt = 0;
	uint64	totsz = 0;
	for(int i = foldercnt; i; --i){
		sprintf(fldname, "/%08u", i);
		*fname = 0;
		for(int j = filecnt; j; --j){
			sprintf(fname, "/%08u.txt", j);
			++cnt;
			fdq.push_back(FileDevice());
			if(fdq.back().open(name, FileDevice::RW)){
				cout<<"error "<<strerror(errno)<<" "<<cnt<<endl;
				cout<<"failed to open file "<<name<<endl;
				return 0;
			}else{
				//cout<<"name = "<<name<<" size = "<<fdq.back().size()<<endl;
				totsz += fdq.back().size();
			}
		}
	}
	cout<<"fdq size = "<<fdq.size()<<" total size "<<totsz<<endl;
	//return 0;
	MyWorkPool wp;
	wp.start(4);
	for(FileDeuqeT::iterator it(fdq.begin()); it != fdq.end(); ++it){
		wp.push(&(*it));
	}
	wp.stop(true);
	Thread::waitAll();
	return 0;
}

