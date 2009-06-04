#include "system/dynamictype.hpp"
#include "testcommands.hpp"
#include "system/debug.hpp"
#include "system/thread.hpp"
#include "system/cassert.hpp"
#include <iostream>

using namespace std;

#include <vector>

namespace test_cmds{

class FirstCommand;
class SecondCommand;
class ThirdCommand;//Inherits Second

}


class MyObject: public DynamicReceiver<MyObject>{
public:
	
	static void dynamicRegister(DynamicMap &_rdm);

	int dynamicReceive(test_cmds::FirstCommand &_rcmd);
	int dynamicReceive(test_cmds::SecondCommand &_rcmd);
};

#include "testcommands.hpp"

void MyObject::dynamicRegister(DynamicMap &_rdm){
	tdbgi(Dbg::any,"");
	BaseTp::dynamicRegister<test_cmds::FirstCommand>(_rdm);
	BaseTp::dynamicRegister<test_cmds::SecondCommand>(_rdm);
	//BaseTp::dynamicRegister<test_cmds::ThirdCommand>(_rdm);
	tdbgo(Dbg::any,"");
}

int some_test(int _v, const char *_str){
	tdbgi(Dbg::any,_v<<','<<_str);
	int rval = _v + strlen(_str);
	Thread::sleep(1000);
	tdbgo(Dbg::any,rval);
	return rval;
}

int MyObject::dynamicReceive(test_cmds::FirstCommand &_rcmd){
	tdbgi(Dbg::any,_rcmd.v);
	idbg("First command value "<<_rcmd.v);
	some_test(10, "some string");
	tdbgo(Dbg::any,0);
	return 0;
}

int MyObject::dynamicReceive(test_cmds::SecondCommand &_rcmd){
	idbg("Second command value "<<_rcmd.v);
	return 0;
}


int main(){
#ifdef UDEBUG
	Dbg::instance().levelMask();
	Dbg::instance().moduleMask();
	Dbg::instance().initStdErr(false);
#endif
	idbg("sizeof(uint8)     = "<<sizeof(uint8));
	idbg("sizeof(int8)      = "<<sizeof(int8));
	idbg("sizeof(uint16)    = "<<sizeof(uint16));
	idbg("sizeof(int16)     = "<<sizeof(int16));
	idbg("sizeof(uint32)    = "<<sizeof(uint32));
	idbg("sizeof(int32)     = "<<sizeof(int32));
	idbg("sizeof(uint64)    = "<<sizeof(uint64));
	idbg("sizeof(int64)     = "<<sizeof(int64));
	idbg("sizeof(uint)      = "<<sizeof(uint));
	idbg("sizeof(ulong)     = "<<sizeof(ulong));
	idbg("sizeof(longlong)  = "<<sizeof(longlong));
	idbg("sizeof(ulonglong) = "<<sizeof(ulonglong));
	MyObject o;
	cout<<"gigi"<<endl;
	test_cmds::FirstCommand		c1(10);
	test_cmds::SecondCommand	c2("second");
	test_cmds::ThirdCommand		c3('3', "third", 30);
	
	o.dynamicReceiver(static_cast<DynamicBase*>(&c1));
	o.dynamicReceiver(static_cast<DynamicBase*>(&c2));
	o.dynamicReceiver(static_cast<DynamicBase*>(&c3));
	
	return 0;
}


