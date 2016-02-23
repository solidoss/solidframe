#include "serialization/typeidmap.hpp"
#include "serialization/binarybasic.hpp"
#include "system/exception.hpp"
#include <iostream>

using namespace std;
using namespace solid;
using namespace solid::serialization;

bool test(const uint32 _protocol_id, const uint64 _message_id, bool _should_fail_join = false){
	
	uint64  tid;
	
	if(!joinTypeId(tid, _protocol_id, _message_id) and !_should_fail_join){
		THROW_EXCEPTION("fail join");
		return false;
	}else if(_should_fail_join){
		return true;
	}
	
	cout<<"crossSize("<<tid<<") = "<<binary::crossSize(tid)<<endl;
	
	uint32 pid;
	uint64 mid;
	
	splitTypeId(tid, pid, mid);
	
	if(mid == _message_id and pid == _protocol_id){
		return true;
	}else{
		THROW_EXCEPTION("fail split");
		return false;
	}
}

int test_typeidjoin(int argc, char *argv[]){
	
	test(0, 0);
	test(1, 1);
	test(2, 2);
	test(0xff, 0xff);
	test(0xffff, 0xffff);
	test(0xffffff, 0xffffff);
	test(0xffffffff, 0xffffffff, true);
	test(0xffff, 0xffffffffff);
	return 0;
}