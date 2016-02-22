#include <iostream>
#include <bitset>
#include "utility/algorithm.hpp"
#include "utility/common.hpp"
#include "system/cassert.hpp"
#include "system/exception.hpp"
#include "serialization/binarybasic.hpp"

using namespace std;
using namespace solid;
using namespace solid::serialization::binary;
//---

void print(uint32 _v){
	bitset<32>		bs(_v);
	cout<<bs<<": max_bit_count = "<<max_bit_count(_v);
	cout<<" max_padded_bit_cout = "<<max_padded_bit_cout(_v);
	cout<<" max_padded_byte_cout = "<<max_padded_byte_cout(_v)<<endl;
}


//=============================================================================

//=============================================================================

template <typename T>
bool test(const T &_v, const size_t _estimated_size){
	char 		tmp[256] = {0};
	size_t		sz = crossSize(_v);
	if(sz != _estimated_size){
		THROW_EXCEPTION("error");
		return false;
	}
	char 		*p = crossStore(tmp, _v);
	if(p == nullptr){
		THROW_EXCEPTION("error");
		return false;
	}
	
	if((p - tmp) != _estimated_size){
		THROW_EXCEPTION("error");
		return false;
	}
	
	if(crossSize(tmp) != _estimated_size){
		THROW_EXCEPTION("error");
		return false;
	}
	
	T	v;
	
	const char *cp;
	
	cp = crossLoad(tmp, v);
	
	if(cp == nullptr){
		THROW_EXCEPTION("error");
		return false;
	}
	
	if((cp - tmp) != _estimated_size){
		THROW_EXCEPTION("error");
		return false;
	}
	
	if(v != _v){
		THROW_EXCEPTION("error");
		return false;
	}
	return true;
}

int test_binarybasic(int argc, char *argv[]){
	cout<<"max uint8 value with crc: "<<(int)max_value_without_crc_8()<<endl;
	
	print(0);
	print(1);
	print(2);
	print(3);
	print(128);
	print(255);
	print(256);
	print(0xffff);
	print(0xfffff);
	print(0xffffff);
	print(0xfffffff);
	print(0xffffffff);
	
	
	if(!test(static_cast<uint8>(0x00), 1)){
		cassert(false);
	}
	
	if(!test(static_cast<uint8>(0x1), 2)){
		cassert(false);
	}
	
	if(!test(static_cast<uint8>(0xff), 2)){
		cassert(false);
	}
	
	if(!test(static_cast<uint16>(0), 1)){
		cassert(false);
	}
	if(!test(static_cast<uint16>(0xff), 2)){
		cassert(false);
	}
	if(!test(static_cast<uint16>(0xffff), 3)){
		cassert(false);
	}
	
	if(!test(static_cast<uint32>(0), 1)){
		cassert(false);
	}
	if(!test(static_cast<uint32>(0xff), 2)){
		cassert(false);
	}
	if(!test(static_cast<uint32>(0xffff), 3)){
		cassert(false);
	}
	if(!test(static_cast<uint32>(0xffffff), 4)){
		cassert(false);
	}
	if(!test(static_cast<uint32>(0xffffffff), 5)){
		cassert(false);
	}
	
	if(!test(static_cast<uint64>(0), 1)){
		cassert(false);
	}
	if(!test(static_cast<uint64>(0xff), 2)){
		cassert(false);
	}
	if(!test(static_cast<uint64>(0xffff), 3)){
		cassert(false);
	}
	if(!test(static_cast<uint64>(0xffffff), 4)){
		cassert(false);
	}
	if(!test(static_cast<uint64>(0xffffffff), 5)){
		cassert(false);
	}
	if(!test(static_cast<uint64>(0xffffffffffULL), 6)){
		cassert(false);
	}
	if(!test(static_cast<uint64>(0xffffffffffffULL), 7)){
		cassert(false);
	}
	if(!test(static_cast<uint64>(0xffffffffffffffULL), 8)){
		cassert(false);
	}
	if(!test(static_cast<uint64>(0xffffffffffffffffULL), 9)){
		cassert(false);
	}
	return 0;
}
