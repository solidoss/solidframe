/* Implementation file debug.cpp
	
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

#include "debug.h"
#include "directory.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include "thread.h"

#ifdef UDEBUG
const int Dbg::fileoff = (strstr(__FILE__, "system/src") - __FILE__);//sizeof("workspace/") - 1 == 9
#endif

void initDebug(const char *_prefix){
	#ifdef UDEBUG
	std::ios_base::sync_with_stdio (false);
	Directory::create("dbg");
	char *name = new char[strlen(_prefix)+50];
	sprintf(name,"%s%u.dbg",_prefix,getpid());
	printf("debug file: [%s]\r\n",name);
	int fd = open(name, O_CREAT | O_RDWR | O_TRUNC, 0600);
	if(dup2(fd,fileno(stderr))<0){
		printf("error duplicating filedescriptor\n");
	}
	#endif
}

#ifdef UDEBUG
static Mutex gmut;

void Dbg::lock(TimeSpec &t){
	clock_gettime(CLOCK_MONOTONIC, &t);
	gmut.lock();
}
void Dbg::unlock(){gmut.unlock();}
int Dbg::crtThrId(){return Thread::currentid();}
#endif

