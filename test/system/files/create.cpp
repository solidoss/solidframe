/* Implementation file create.cpp
	
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
#include <cstring>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/exception.hpp>
#include <boost/utility.hpp>

using namespace std;
///\cond 0
int createFile(const char *_nm, const string &_str, ulong _sz);
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
	for(int i = foldercnt; i; --i){
		sprintf(fldname, "/%08u", i);
		boost::filesystem::create_directory(name);
		*fname = 0;
		for(int j = filecnt; j; --j){
			sprintf(fname, "/%08u.txt", j);
			ulong sz = (filecnt * minsz + (j * (maxsz - minsz)))/filecnt;
			cout<<"name = "<<name<<" size = "<<sz<<endl;
			if(createFile(name, line, sz)){
				cout<<"failed create file"<<endl;
			}
		}
	}
	return 0;
}


int createFile(const char *_nm, const string &_str, ulong _sz){
	ofstream ofs(_nm);
	if(!ofs) return -1;
	ulong osz = 0;
	ulong i = 0;
	while(osz <= _sz){
		ofs<<osz<<' '<<_str<<endl;
		osz += _str.size();
		++i;
	}
	return 0;
}


