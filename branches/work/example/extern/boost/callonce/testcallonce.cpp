#include <boost/thread/once.hpp>
#include <boost/thread/thread.hpp>
#include <iostream>

using namespace std;

struct Singleton{
	static void once_cbk();
	static	Singleton& instance();
	Singleton():val(0){
		sleep(10);
		val = 1;
	}
	int val;
};


void Singleton::once_cbk(){
	instance();
}

Singleton& Singleton::instance(){
	static boost::once_flag once = BOOST_ONCE_INIT;
	static Singleton s;
	boost::call_once(&once_cbk, once);
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
