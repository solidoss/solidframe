#include <iostream>
#include <functional>
#include "system/socketdevice.hpp"
#include "boost/function.hpp"
#include <mcheck.h>
#include <malloc.h>

using namespace std;
using namespace solid;


#define TEST_ALLOC
#ifdef TEST_ALLOC
static void *(*old_malloc_hook)(size_t, const void *);

static void *new_malloc_hook(size_t size, const void *caller) {
    void *mem;

    __malloc_hook = old_malloc_hook;
    mem = malloc(size);
    fprintf(stderr, "%p: malloc(%zu) = %p\n", caller, size, mem);
    __malloc_hook = new_malloc_hook;

    return mem;
}

static void init_my_hooks(void) {
    old_malloc_hook = __malloc_hook;
    __malloc_hook = new_malloc_hook;
}

void (*__MALLOC_HOOK_VOLATILE __malloc_initialize_hook)(void) = init_my_hooks;

void* operator new(size_t sz) throw (std::bad_alloc)
{
    cerr << "allocating " << sz << " bytes\n";
    void* mem = malloc(sz);
    if (mem)
        return mem;
    else
        throw std::bad_alloc();
}


void operator delete(void* ptr) throw()
{
    cerr << "deallocating at " << ptr << endl;
    free(ptr);
}
#endif


struct ReactorContext{
	void print(){
		cout<<"ReactorContext this = "<<(void*)this<<endl;
	}
};

struct Event{
	
};

class Listener{
public:
	Listener(){}
	
	template <typename F>
	bool accept(ReactorContext &_rctx, F _f){
		fnc = _f;
		return true;
	}
	
	bool call(ReactorContext &_rctx){
		SocketDevice sd;
		return fnc(_rctx, sd);
	}
	
private:
	typedef boost::function<bool(ReactorContext&, SocketDevice &)>	FunctionT;
	FunctionT	fnc;
};

struct Big{
	char buf[312];
};

class Object{
public:
	Object(Listener &_rlsn):rlsn(_rlsn){}
	
	bool onEvent(ReactorContext &_rctx, Event const &_revent){
		auto f = std::bind(&Object::onAccept, this, std::placeholders::_1, std::placeholders::_2, Big());
		cerr<<"sizeof(f) = "<<sizeof(f)<<endl;
		rlsn.accept(_rctx, f);
		return true;
	}
	bool onAccept(ReactorContext &_rctx, SocketDevice &_rsd, Big &_big){
		_rctx.print();
		cout<<"On Accept"<<endl;
		return true;
	}
private:
	Listener	&rlsn;
};


int main(int argc, char *argv[]){
	cerr<<"================Step1"<<endl;
	Listener	lsn;
	cerr<<"================Step2"<<endl;
	Object		obj(lsn);
	cerr<<"================Step3"<<endl;
	ReactorContext	ctx;
	cerr<<"================Step4"<<endl;
	ctx.print();
	cerr<<"================Step5"<<endl;
	obj.onEvent(ctx, Event());
	cerr<<"================Step6"<<endl;
	
	lsn.call(ctx);
	cerr<<"================Step7"<<endl;
	
	
	return 0;
}