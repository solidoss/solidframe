#include <iostream>
#include "boost/atomic.hpp"
#include "system/common.hpp"
#include <deque>
#include <vector>


using namespace std;
using namespace solid;


int main(int argc, char *argv[]){
	
	boost::atomic<int>		aint;
	boost::atomic<uint32>	auint32;
	boost::atomic<uint64>	auint64;
	boost::atomic<void*>	avoid;
	boost::atomic<string>	astring;
	
	
	
	cout<<"is_lock_free<int>: "<<aint.is_lock_free()<<endl;
	cout<<"is_lock_free<uint32>: "<<auint32.is_lock_free()<<endl;
	cout<<"is_lock_free<uint64>: "<<auint64.is_lock_free()<<endl;
	cout<<"is_lock_free<void*>: "<<avoid.is_lock_free()<<endl;
	cout<<"is_lock_free<string>: "<<astring.is_lock_free()<<endl;
	
	aint.store(10);
	
	int v = aint.load();
	
	return 0;
}

