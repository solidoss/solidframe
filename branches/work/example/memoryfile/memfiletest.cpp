#include "system/debug.hpp"
#include "system/common.hpp"
#include "utility/memoryfile.hpp"
#include "system/filedevice.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <ctime>
using namespace std;



enum{
	BUFFER_CAP = 20 * 1024
};

int randomsize(){
	uint r = rand();
	return r % BUFFER_CAP;
}


int main(int argc, char *argv[]){
	if(argc != 2){
		cout<<"Expected a filepath as parameter"<<endl;
		return 0;
	}
	int stime;
	long ltime;

	ltime = static_cast<long>(time(NULL)); /* get current calendar time */
	stime = (unsigned int) ltime/2;
	srand(stime);
	
	MemoryFile mf;
	{
		//first we read a file in memory:
		FileDevice fd;
		if(fd.open(argv[1], FileDevice::RO) != OK){
			cout<<"unable to open file"<<endl;
			return 0;
		}
		cout<<"File size: "<<fd.size()<<endl;
		char b[20 * 1024];
		int rv;
		int rc;
		do{
			rv = 2 * 1024;//randomsize();
			//cout<<rv<<endl;
			rc = fd.read(b, rv);
			if(rc < 0) break;
			mf.write(b, rc);
		}while(rc == rv);
		cout<<"error "<<strerror(errno)<<endl;
	}
	
	{
		//then we write it back to disk
		
		FileDevice fd;
		if(fd.open("test.dat", FileDevice::WO | FileDevice::CR | FileDevice::TR) != OK){
			cout<<"unable to open file"<<endl;
			return 0;
		}
		mf.seek(0, SeekBeg);
		char b[20 * 1024];
		int rv, rc;
		do{
			rv = 2 * 1024;//randomsize();
			//cout<<rv<<endl;
			rc = mf.read(b, rv);
			fd.write(b, rc);
		}while(rc == rv);
	}
	string s("sha1sum ");
	s+= argv[1];
	system(s.c_str());
	s = "sha1sum ";
	s += "test.dat";
	system(s.c_str());
	return 0;
}
