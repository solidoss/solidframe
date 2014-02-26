// open.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <iostream>
#include <fstream>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/exception.hpp>
#include <boost/utility.hpp>
#include "system/filedevice.hpp"
#include <deque>
#include <cstring>
#include <cerrno>

using namespace std;
using namespace solid;

///\cond 0
typedef std::deque<FileDevice>	FileDeuqeT;
typedef std::deque<auto_ptr<FileDevice> >	AutoFileDequeT;
///\endcond


int main(int argc, char *argv[]){

// 	AutoFileDequeT aq;
// 	aq.push_back(auto_ptr<FileDevice>());
	if(argc != 4){
		cout<<"./file_open_pool /path/to/folder file-count folder-count"<<endl;
		return 0;
	}
	char c;
	char name[1024];
	//int minsz = atoi(argv[2]);
	//int maxsz = atoi(argv[3]);
	int filecnt   = atoi(argv[2]);
	int foldercnt   = atoi(argv[3]);
	string line;
	for(char c = '0'; c <= '9'; ++c) line += c;
	for(char c = 'a'; c <= 'z'; ++c) line += c;
	for(char c = 'A'; c <= 'Z'; ++c) line += c;
	cout<<"line"<<endl<<line<<endl;
	sprintf(name, "%s", argv[1]);
	char *fldname = name + strlen(argv[1]);
	char *fname = name + strlen(argv[1]) + 1 + 8;
	FileDeuqeT	fdq;
	int cnt = 0;
	cout<<"insert a char: ";cin>>c;
	for(int i = foldercnt; i; --i){
		sprintf(fldname, "/%08u", i);
		*fname = 0;
		for(int j = filecnt; j; --j){
			sprintf(fname, "/%08u.txt", j);
			++cnt;
			//ulong sz = (filecnt * minsz + (j * (maxsz - minsz)))/filecnt;
			fdq.push_back(FileDevice());
			if(fdq.back().open(name, FileDevice::ReadWriteE)){
				cout<<"error "<<strerror(errno)<<" "<<cnt<<endl;
				cout<<"failed to open file "<<name<<endl;
				return 0;
			}else{
				cout<<"name = "<<name<<" size = "<<fdq.back().size()<<endl;
			}
		}
	}
	cout<<"fdq size = "<<fdq.size()<<endl;
	char bf[1000];
	for(FileDeuqeT::iterator it(fdq.begin()); it != fdq.end(); ++it){
		if(it->read(bf, 1000) != 1000){
			cout<<"error reading "<<(int)(it - fdq.begin())<<endl;
			break;
		}
	}
	cout<<"insert a char: ";cin>>c;
	return 0;
}

