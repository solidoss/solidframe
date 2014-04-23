// create.cpp
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
#include <cstring>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/exception.hpp>
#include <boost/utility.hpp>
#include "system/common.hpp"

using namespace std;
///\cond 0
int createFile(const char *_nm, const string &_str, solid::ulong _sz);
///\endcond

int main(int argc, char *argv[]){
	if(argc != 6){
		cout<<"./create /path/to/folder from-size to-size file-count folder-count"<<endl;
		return 0;
	}
	char name[1024];
	int minsz = atoi(argv[2]);
	int maxsz = atoi(argv[3]);
	int filecnt   = atoi(argv[4]);
	int foldercnt   = atoi(argv[5]);
	string line;
	for(char c = '0'; c <= '9'; ++c) line += c;
	for(char c = 'a'; c <= 'z'; ++c) line += c;
	for(char c = 'A'; c <= 'Z'; ++c) line += c;
	cout<<"line"<<endl<<line<<endl;
	sprintf(name, "%s", argv[1]);
	char *fldname = name + strlen(argv[1]);
	char *fname = name + strlen(argv[1]) + 1 + 8;
	if(*(fldname - 1) == '/') --fldname;
	double maxsize = maxsz;
	double minsize = minsz;
	double filecount = filecnt;
	for(int i = foldercnt; i; --i){
		sprintf(fldname, "/%08u", i);
		boost::filesystem::create_directory(name);
		*fname = 0;
		for(int j = filecnt; j; --j){
			sprintf(fname, "/%08u.txt", j);
			double sz = minsize + ((maxsize - minsize)*(j - 1))/(filecount - 1);
			solid::ulong size = sz;
			cout<<"name = "<<name<<" size = "<<size<<" sz "<<sz<<endl;
			if(createFile(name, line, size)){
				cout<<"failed create file"<<endl;
			}
		}
	}
	return 0;
}


int createFile(const char *_nm, const string &_str, solid::ulong _sz){
	ofstream ofs(_nm);
	if(!ofs) return -1;
	solid::ulong osz = 0;
	solid::ulong i = 0;
	while(osz <= _sz){
		ofs<<osz<<' '<<_str<<endl;
		osz += _str.size();
		++i;
	}
	return 0;
}


