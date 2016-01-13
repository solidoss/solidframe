#include <iostream>
#include <cstdio>
#include <deque>
#include <bitset>
#include "system/common.hpp"
using namespace solid;
using namespace std;

/*
 Builds a table of strings containing either the character itself or its name
*/

unsigned char rotate(unsigned char _c){
	unsigned char c(0);
	c |= (_c & 1) << 7;
	_c >>= 1;
	c |= (_c & 1) << 6;
	_c >>= 1;
	c |= (_c & 1) << 5;
	_c >>= 1;
	c |= (_c & 1) << 4;
	_c >>= 1;
	c |= (_c & 1) << 3;
	_c >>= 1;
	c |= (_c & 1) << 2;
	_c >>= 1;
	c |= (_c & 1) << 1;
	_c >>= 1;
	c |= (_c & 1) << 0;
	return c;
}

int main(){
	typedef std::deque<string>	StringVectorT;
	typedef std::bitset<8>		BitSet8T;
	StringVectorT sv;
	char buf[32];
	for(uint32 i = 0; i < 256; ++i){
		unsigned char c = (unsigned char)i;
		unsigned char rc = rotate(c);
		sprintf(buf, "0x%02X", rc);
		sv.push_back(string(buf));
		BitSet8T b1(c);
		BitSet8T b2(rc);
		cout<<b1<<" -> "<<b2<<endl;
	}
	solid::uint maxsz = 0;
	for(StringVectorT::const_iterator it(sv.begin()); it != sv.end(); ++it){
		if(it->size() > maxsz){
			maxsz = it->size();
		}
	}
	const char spaces[]= "                                                                ";
	for(StringVectorT::const_iterator it(sv.begin()); it != sv.end(); ++it){
		if(!((it - sv.begin()) % 10)) cout<<endl;
		uint32 spno = maxsz - it->size();
		cout.write(spaces, spno);
		cout<<*it<<',';
	}
	cout<<endl;
	return 0;
}
