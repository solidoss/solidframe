#include <iostream>
#include <bitset>
#include "utility/common.hpp"

using namespace std;


int main(){
	const uint8 u8v[] = {
		1,2,3,4,5,6,7,8,
		0xff-1,0xff-2,0xff-3,0xff-4,0xff-5,0xff-6,0xff-7,0xff-8,0xff-9,
		0
	};
	const uint16 u16v[] = {
		1,2,3,4,5,6,7,8,
		0xffff-1,0xffff-2,0xffff-3,0xffff-4,0xffff-5,0xffff-6,0xffff-7,0xffff-8,
		0xffff-9,0xffff-10,0xffff-11,0xffff-12,0xffff-13,0xffff-14,0xffff-15,0xffff-16,
		0xffff-17,
		0
	};
	const uint32 u32v[] = {
		1,2,3,4,5,6,7,8,
		0xffffffff-1,0xffffffff-2,0xffffffff-3,0xffffffff-4,0xffffffff-5,0xffffffff-6,0xffffffff-7,0xffffffff-8,
		0xffffffff-9,0xffffffff-10,0xffffffff-11,0xffffffff-12,0xffffffff-13,0xffffffff-14,0xffffffff-15,0xffffffff-16,
		0xffffffff-17,0xffffffff-18,0xffffffff-19,0xffffffff-20,0xffffffff-21,0xffffffff-22,0xffffffff-23,0xffffffff-24,
		0xffffffff-25,0xffffffff-26,0xffffffff-27,0xffffffff-28,0xffffffff-29,0xffffffff-30,
		0xffffffff-31,0xffffffff-32,0xffffffff-33,
		0
	};
	typedef std::bitset<8>		BitSet8T;
	typedef std::bitset<16> 	BitSet16T;
	typedef std::bitset<32> 	BitSet32T;
	typedef CRCValue<uint8>		CRCValue8T;
	typedef CRCValue<uint16>	CRCValue16T;
	typedef CRCValue<uint32>	CRCValue32T;
	
	const uint8 *pu8(u8v);
	do{
		CRCValue8T	v(*pu8);
		BitSet8T	bsv((ulong)*pu8);
		BitSet8T	bs((uint8)v);
		cout<<(uint32)*pu8<<" = "<<bsv<<" ok = "<<v.ok()<<" crc = "<<(ulong)v.crc()<<" value = "<<(ulong)v.value()<<" storage = "<<bs<<endl;
	}while(*(++pu8));
	
	cout<<endl<<endl;
	const uint16 *pu16(u16v);
	do{
		CRCValue16T	v(*pu16);
		BitSet16T	bsv((ulong)*pu16);
		BitSet16T	bs((uint16)v);
		cout<<(uint32)*pu16<<" = "<<bsv<<" ok = "<<v.ok()<<" crc = "<<(ulong)v.crc()<<" value = "<<(ulong)v.value()<<" storage = "<<bs<<endl;		
	}while(*(++pu16));
	
	cout<<endl<<endl;
	const uint32 *pu32(u32v);
	do{
		CRCValue32T		v(*pu32);
		BitSet32T		bsv((ulong)*pu32);
		BitSet32T		bs((uint32)v);
		cout<<(uint32)*pu32<<" = "<<bsv<<" ok = "<<v.ok()<<" crc = "<<(ulong)v.crc()<<" value = "<<(ulong)v.value()<<" storage = "<<bs<<endl;				
	}while(*(++pu32));
	return 0;
}
