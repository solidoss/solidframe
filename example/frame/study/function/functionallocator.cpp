#include <iostream>
#include "system/function.hpp"
#include <mcheck.h>
#include <malloc.h>

using namespace std;

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

struct Context{
	
};

struct Event{
	
};

struct Allocator{
	void* allocate(size_t _n){
		return new char[_n];
	}
	void dealocate(void *_p, size_t _n){
		delete []static_cast<char*>(_p);
	}
};

typedef FUNCTION_NS::function<void(Context&)>	FunctionT;


struct Stub{
	template <typename T>
	void set(T _t){
		f = _t;
	}
	
	void call(Context &_rctx){
		f(_rctx);
	}
	FunctionT	f;
};

struct Test{
	std::string txt;
	size_t		v;
	
	Test():v(0){}
	Test(string const &_rtxt, size_t _v = 0):txt(_rtxt), v(_v){}
	
	void operator()(Context &_rctx){
		cout<<"txt = "<<txt<<" v = "<<v<<endl;
	}
	
};

int main(int argc, char *argv[]){
	cout<<"Sizeof(FunctionT) = "<<sizeof(FunctionT)<<endl;
	Stub	s;
	Context ctx;
	cout<<"Setting the functor"<<endl;
	s.set(Test("ceva", 10));
	cout<<"Calling the functor"<<endl;
	s.call(ctx);
	FunctionT f;
	f = move(s.f);
	cout<<"s.f valid = "<<(bool)s.f<<endl;
	f(ctx);
	return 0;
}
