#include <iostream>
#include <deque>
#include "system/common.hpp"
using namespace std;

/*
 Builds a table of strings containing either the character itself or its name
*/

int main(){
	typedef std::deque<string>	StringVectorTp;
	StringVectorTp sv;
	char buf[32];
	for(uint32 i = 0; i < 256; ++i){
		//sv.push_back(string());
		if(i == ' '){
			sprintf(buf, "\"SPACE\"");
		}else if(i == '\t'){
			sprintf(buf, "\"TAB\"");
		}else if(i == '\r'){
			sprintf(buf, "\"CR\"");
		}else if(i == '\n'){
			sprintf(buf, "\"LF\"");
		}else if(isgraph(i)){	
			sprintf(buf, "\"%c\"", (unsigned char)i);
		}else{
			sprintf(buf, "\"Ox%02X\"", (unsigned char)i);
		}
		sv.push_back(string(buf));
		//cout<<buf<<endl;
	}
	uint maxsz = 0;
	for(StringVectorTp::const_iterator it(sv.begin()); it != sv.end(); ++it){
		if(it->size() > maxsz){
			maxsz = it->size();
		}
	}
	const char spaces[]= "                                                                ";
	for(StringVectorTp::const_iterator it(sv.begin()); it != sv.end(); ++it){
		if(!((it - sv.begin()) % 10)) cout<<endl;
		uint32 spno = maxsz - it->size();
		cout.write(spaces, spno);
		cout<<*it<<',';
	}
	cout<<endl;
	return 0;
}
