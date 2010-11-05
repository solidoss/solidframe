#include <iostream>
#include "utility/holder.hpp"
#include "system/debug.hpp"
using namespace std;


struct BasicBuffer{
	BasicBuffer(char *_pb = NULL, uint32 _bl = 0):pbeg(_pb), pend(_pb + _bl){
		idbg("");
	}
	BasicBuffer(char *_pb, char *_pe):pbeg(_pb), pend(_pe){
		idbg("");
	}
	virtual ~BasicBuffer(){
		idbg("");
	}
	mutable char	*pbeg;
	char			*pend;
};

struct AllocateBuffer: BasicBuffer{
	AllocateBuffer(uint32 _sz = 1024):BasicBuffer(new char[_sz], _sz){
		idbg("");
	}
	~AllocateBuffer(){
		delete []pbeg;
	}
	char *release()const{
		char *tmp(pbeg);
		pbeg = NULL;
		return tmp;
	}
	AllocateBuffer(const AllocateBuffer &_rb):BasicBuffer(_rb.release(), _rb.pend){
		idbg("");
	}
};

struct DeleteBuffer: BasicBuffer{
	DeleteBuffer(char *_pb = NULL, uint32 _bl = 0):BasicBuffer(_pb, _bl){
		idbg("");
	}
	virtual ~DeleteBuffer(){
		idbg("");
		delete []pbeg;
	}
	char *release()const{
		char *tmp(pbeg);
		pbeg = NULL;
		return tmp;
	}
	DeleteBuffer(const DeleteBuffer &_rb):BasicBuffer(_rb.release(), _rb.pend){
		idbg("");
	}
};

struct Test:BasicBuffer{
	char buf[128];
};

int main(){
#ifdef UDEBUG
	{
	string s;
	Dbg::instance().levelMask();
	Dbg::instance().moduleMask();
	Dbg::instance().initStdErr(false, &s);
	cout<<"Debug output: "<<s<<endl;
	s.clear();
	Dbg::instance().moduleBits(s);
	cout<<"Debug bits: "<<s<<endl;
	}
#endif
	char		buf[512];
	Holder<BasicBuffer>		h;
	cout<<(void*)h->pbeg<<' '<<(void*)h->pend<<endl;
	h = BasicBuffer(buf, 512);
	cout<<(void*)h->pbeg<<' '<<(void*)h->pend<<endl;
	h = AllocateBuffer(256);
	cout<<(void*)h->pbeg<<' '<<(void*)h->pend<<endl;
	Holder<BasicBuffer>		h2(DeleteBuffer(new char[128], 128));
	cout<<(void*)h2->pbeg<<' '<<(void*)h2->pend<<endl;
	
	//h = Test();
	return 0;
}

