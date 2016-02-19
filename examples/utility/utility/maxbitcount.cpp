#include <iostream>
#include <bitset>
#include "utility/common.hpp"
using namespace std;
using namespace solid;


unsigned int v;  // find the number of trailing zeros in 32-bit v 
int r;           // result goes here
static const int MultiplyDeBruijnBitPosition[32] = 
{
  0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8, 
  31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
};

int trailing_zeros(unsigned int v){
	return MultiplyDeBruijnBitPosition[((uint32_t)((v & -v) * 0x077CB531U)) >> 27];
}

int main(){
	cout<<"========================================"<<endl;
	for(uint8_t i = 0; i < 100; ++i){
		bitset<8>	bs(i);
		cout<<bs<<" = "<<leading_zero_count(i)<<endl;
	}
	{
		bitset<8>	bs(static_cast<uint8_t>(-1));
		cout<<bs<<" = "<<leading_zero_count(static_cast<uint8_t>(-1))<<endl;
	}
	cout<<"========================================"<<endl;
	for(uint16_t i = 0; i < 1000; ++i){
		bitset<16>	bs(i);
		cout<<bs<<" = "<<leading_zero_count(i)<<endl;
	}
	{
		bitset<16>	bs(static_cast<uint16_t>(-1));
		cout<<bs<<" = "<<leading_zero_count(static_cast<uint16_t>(-1))<<endl;
	}
	cout<<"========================================"<<endl;
	for(uint32_t i = 0; i < 1000; ++i){
		bitset<32>	bs(i);
		cout<<bs<<" = "<<leading_zero_count(i)<<endl;
	}
	{
		bitset<32>	bs(static_cast<uint32_t>(-1));
		cout<<bs<<" = "<<leading_zero_count(static_cast<uint32_t>(-1))<<endl;
	}
	cout<<"========================================"<<endl;
	for(uint64_t i = 0; i < 1000; ++i){
		bitset<64>	bs(i);
		cout<<bs<<" = "<<leading_zero_count(i)<<endl;
	}
	{
		bitset<64>	bs(static_cast<uint64_t>(-1));
		cout<<bs<<" = "<<leading_zero_count(static_cast<uint64_t>(-1))<<endl;
	}
	cout<<"========================================"<<endl;
	return 0;
}                      
                      