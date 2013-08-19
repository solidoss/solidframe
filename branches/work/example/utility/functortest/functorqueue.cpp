#include "system/debug.hpp"
#include "utility/functorqueue.hpp"
#include <string>

using namespace solid;


struct FncAVoid{
	FncAVoid(int _v):v(_v){
		idbg("");
	}
	FncAVoid(const FncAVoid &_rv){
		idbg("");
		*this = _rv;
	}
	~FncAVoid(){
		idbg("");
	}
	int operator()(){
		idbg("v = "<<v);
		return v;
	}
	int v;
	int t;
	int c;
};

struct FncBVoid{
	FncBVoid(int _v):v(_v){
		idbg("");
	}
	FncBVoid(const FncBVoid &_rv){
		idbg("");
		*this = _rv;
	}
	~FncBVoid(){
		idbg("");
	}
	int operator()(){
		idbg("v = "<<v);
		return v;
	}
	int v;
};

struct FncCVoid{
	FncCVoid(int _v):v(_v){
		idbg("");
	}
	FncCVoid(const FncCVoid &_rv){
		idbg("");
		*this = _rv;
	}
	~FncCVoid(){
		idbg("");
	}
	int operator()(){
		idbg("v = "<<v);
		return v;
	}
	int 	v;
	char 	buf[512];
};


int main(int argc, char *argv[]){
#ifdef UDEBUG
	{
		Debug::the().initStdErr();
		Debug::the().levelMask("iwe");
		Debug::the().moduleMask("all");
	}
#endif
	if(1){
		FunctorQueue<>							fq;
		
		
		fq.push(FncAVoid(10));
		
		fq.callFront();
		
		fq.pop();
	}
	if(1){
		FunctorQueue<int>						fq;
		
		
		fq.push(FncAVoid(1));
		fq.push(FncBVoid(2));
		fq.push(FncCVoid(3));
		
		while(fq.size()){
			int rv = fq.callFront();
			fq.pop();
			idbg("rv = "<<rv);
		}
	}
	return 0;
}

