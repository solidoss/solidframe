#include <iostream>
#include <bitset>
#include "utility/common.hpp"

using namespace std;


int main(){
	typedef std::bitset<8>		BitSet8T;
	typedef std::bitset<16> 	BitSet16T;
	typedef std::bitset<32> 	BitSet32T;
	typedef std::bitset<64> 	BitSet64T;
	typedef CRCValue<uint8>		CRCValue8T;
	typedef CRCValue<uint16>	CRCValue16T;
	typedef CRCValue<uint32>	CRCValue32T;
	typedef CRCValue<uint64>	CRCValue64T;
	
	const uint8 m8(CRCValue8T::maximum());
	const uint8 u8v[] = {
		1,2,3,4,5,6,7,8,
		m8, (uint8)(m8 - (uint8)1), (uint8)(m8 - (uint8)2), (uint8)(m8 - (uint8)3),
		0
	};
	const uint16 m16(CRCValue16T::maximum());
	const uint16 u16v[] = {
		1,2,3,4,5,6,7,8,
		(uint16)m16,(uint16)(m16 -1),(uint16)(m16 -2), (uint16)(m16 -3),
		0
	};
	const uint32 m32(CRCValue32T::maximum());
	const uint32 u32v[] = {
		1,2,3,4,5,6,7,8,
		(uint32)m32,(uint32)(m32 -1),(uint32)(m32 -2), (uint32)(m32 -3),
		0
	};
	const uint64 m64(CRCValue64T::maximum());
	const uint64 u64v[] = {
		1ULL,2ULL,3ULL,4ULL,5ULL,6ULL,7ULL,8ULL,
		(uint64)m64,(uint64)(m64 -1),(uint64)(m64 -2), (uint64)(m64 -3),
		0
	};
	
	
	{
		BitSet8T	bs(CRCValue8T::maximum());
		cout<<"maxval uint8 = "<<(int)CRCValue8T::maximum()<<" = "<<bs<<endl;
	}
	{
		BitSet16T	bs(CRCValue16T::maximum());
		cout<<"maxval uint16 = "<<CRCValue16T::maximum()<<" = "<<bs<<endl;
	}
	{
		BitSet32T	bs((int)CRCValue32T::maximum());
		cout<<"maxval uint32 = "<<CRCValue32T::maximum()<<" = "<<bs<<endl;
	}
	{
		BitSet64T	bs(CRCValue64T::maximum());
		cout<<"maxval uint64 = "<<CRCValue64T::maximum()<<" = "<<bs<<endl;
	}
	const uint8 *pu8(u8v);
	do{
		CRCValue8T	v(*pu8);
		BitSet8T	bsv((uint8)*pu8);
		BitSet8T	bs((uint8)v);
		cout<<(uint32)*pu8<<" = "<<bsv<<" ok = "<<v.ok()<<" crc = "<<(ulong)v.crc()<<" value = "<<(ulong)v.value()<<" storage = "<<bs<<endl;
	}while(*(++pu8));
	
	cout<<endl<<endl;
	const uint16 *pu16(u16v);
	do{
		CRCValue16T	v(*pu16);
		BitSet16T	bsv((int)*pu16);
		BitSet16T	bs((uint16)v);
		cout<<(uint32)*pu16<<" = "<<bsv<<" ok = "<<v.ok()<<" crc = "<<(ulong)v.crc()<<" value = "<<(ulong)v.value()<<" storage = "<<bs<<endl;		
	}while(*(++pu16));
	
	cout<<endl<<endl;
	const uint32 *pu32(u32v);
	do{
		CRCValue32T		v(*pu32);
		BitSet32T		bsv((int)*pu32);
		BitSet32T		bs((int)v);
		cout<<(uint32)*pu32<<" = "<<bsv<<" ok = "<<v.ok()<<" crc = "<<(ulong)v.crc()<<" value = "<<(ulong)v.value()<<" storage = "<<bs<<endl;				
	}while(*(++pu32));
	
	cout<<endl<<endl;
	const uint64 *pu64(u64v);
	do{
		CRCValue64T		v(*pu64);
		BitSet64T		bsv(*pu64);
		BitSet64T		bs((uint64)v);
		cout<<(uint64)*pu64<<" = "<<bsv<<" ok = "<<v.ok()<<" crc = "<<(uint64)v.crc()<<" value = "<<(uint64)v.value()<<" storage = "<<bs<<endl;				
	}while(*(++pu64));
	return 0;
}
