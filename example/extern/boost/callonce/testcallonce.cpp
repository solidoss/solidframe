#include <boost/thread/once.hpp>
#include <boost/thread/thread.hpp>
#include <iostream>
#ifdef ON_WINDOWS
#include <Windows.h>
#endif
using namespace std;

struct Singleton{
	static void once_cbk();
	static	Singleton& instance();
	Singleton():val(0){
#ifdef ON_WINDOWS
		Sleep(1000*10);
#else
		sleep(10);
#endif
		val = 1;
	}
	int val;
private:
	static	Singleton& the_instance();
};


void Singleton::once_cbk(){
	the_instance();
}

Singleton& Singleton::instance(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	boost::call_once(&once_cbk, once);
	return the_instance();
}

Singleton& Singleton::the_instance(){
	static Singleton s;
	return s;
}



struct Runner{
	void operator()(){
		cout<<"in runner: "<<Singleton::instance().val<<endl;
	}
};



int main(){
	Runner r;
    boost::thread thrd(r);
	cout<<"in main: "<<Singleton::instance().val<<endl;
	thrd.join();
	return 0;
}
