#include <iostream>
#include <bitset>
#include "utility/algorithm.hpp"
#include "utility/common.hpp"

using namespace std;
using namespace solid;


inline size_t max_bit_count(uint32 _v){
	return 32 - leading_zero_count(_v);
}

inline size_t max_padded_bit_cout(uint32 _v){
	return fast_padded_size(max_bit_count(_v), 3);
}

inline size_t max_padded_byte_cout(uint32 _v){
	return max_padded_bit_cout(_v) >> 3;
}




void print(uint32 _v){
	bitset<32>		bs(_v);
	cout<<bs<<": max_bit_count = "<<max_bit_count(_v);
	cout<<" max_padded_bit_cout = "<<max_padded_bit_cout(_v);
	cout<<" max_padded_byte_cout = "<<max_padded_byte_cout(_v)<<endl;
}

int main(){
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
	
	return 0;
}
