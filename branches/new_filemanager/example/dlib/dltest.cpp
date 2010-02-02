#include "dltesta.hpp"
#include "system/debug.hpp"
#include <dlfcn.h>
#include <iostream>

using namespace std;


int main(){
#ifdef UDEBUG
	{
	string s;
	Dbg::instance().levelMask("iew");
	Dbg::instance().moduleMask();
	Dbg::instance().initStdErr(false, &s);
	cout<<"Debug output: "<<s<<endl;
	s.clear();
	Dbg::instance().moduleBits(s);
	cout<<"Debug modules: "<<s<<endl;
	}
#endif
	typedef Test* (*CreatorT)(int);
	TestA::instance().set(10);

	//load lib b
	void *libb_handle = dlopen("./libdltestb.so", RTLD_LAZY);
	if(!libb_handle){
		idbg("cannot open libdltestb.so");
		return 0;
	}
	void* pf = dlsym(libb_handle, "createB");
	char *error;
	if ((error = dlerror()) != NULL)
	{
		idbg("dlsym error: "<<error);
		return 0;
	}
	CreatorT pbc = reinterpret_cast<CreatorT>(pf);

	Test* ptb = (*pbc)(2);
	ptb->print();
	delete ptb;

	//load lib c
	void *libc_handle = dlopen("./libdltestc.so", RTLD_LAZY);
	if(!libc_handle){
		idbg("cannot open libdltestc.so");
		return 0;
	}
	pf = dlsym(libc_handle, "createC");
	if ((error = dlerror()) != NULL)
	{
		idbg("dlsym error: "<<error);
		return 0;
	}
	pbc = reinterpret_cast<CreatorT>(pf);

	Test* ptc = (*pbc)(3);
	ptc->print();
	delete ptc;
	
	dlclose(libb_handle);
	dlclose(libc_handle);
	return 0;
}

