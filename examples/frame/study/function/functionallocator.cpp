#include <iostream>
//#include "system/function.hpp"
//#include "boost/function.hpp"
#include "function.hpp"
#include "boost/pool/pool_alloc.hpp"
#include <mcheck.h>
#include <malloc.h>

#define FUNCTION_NS func

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

template <class T>
class Allocator {
	public:
	// type definitions
	typedef T				value_type;
	typedef T*				pointer;
	typedef const T*		const_pointer;
	typedef T&       reference;
	typedef const T& const_reference;
	typedef std::size_t    size_type;
	typedef std::ptrdiff_t difference_type;

	// rebind allocator to type U
	template <class U>
	struct rebind {
		typedef Allocator<U> other;
	};

	// return address of values
	pointer address (reference value) const {
		return &value;
	}
	const_pointer address (const_reference value) const {
		return &value;
	}

	/* constructors and destructor
	* - nothing to do because the allocator has no state
	*/
	Allocator() throw() {
	}
	Allocator(const Allocator&) throw() {
	}
	template <class U>
	Allocator (const Allocator<U>&) throw() {
	}
	~Allocator() throw() {
	}

	// return maximum number of elements that can be allocated
	size_type max_size () const throw() {
		return std::numeric_limits<std::size_t>::max() / sizeof(T);
	}

	// allocate but don't initialize num elements of type T
	pointer allocate (size_type num, const void* = 0) {
		// print message and allocate memory with global new
		std::cerr << "allocate " << num << " element(s)"
					<< " of size " << sizeof(T) << std::endl;
		pointer ret = (pointer)(::operator new(num*sizeof(T)));
		std::cerr << " allocated at: " << (void*)ret << std::endl;
		return ret;
	}

	// initialize elements of allocated storage p with value value
	void construct (pointer p, const T& value) {
		// initialize memory with placement new
		new((void*)p)T(value);
	}

	// destroy elements of initialized storage p
	void destroy (pointer p) {
		// destroy objects by calling their destructor
		p->~T();
	}

	// deallocate storage p of deleted elements
	void deallocate (pointer p, size_type num) {
		// print message and deallocate memory with global delete
		std::cerr << "deallocate " << num << " element(s)"
					<< " of size " << sizeof(T)
					<< " at: " << (void*)p << std::endl;
		::operator delete((void*)p);
	}
};

// return that all specializations of this allocator are interchangeable
template <class T1, class T2>
bool operator== (const Allocator<T1>&,
				const Allocator<T2>&) throw() {
	return true;
}
template <class T1, class T2>
bool operator!= (const Allocator<T1>&,
				const Allocator<T2>&) throw() {
	return false;
}


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
	size_t		v1;
	size_t		v2;
	
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
	s.set(Test("something", 10));
	cout<<"Calling the functor"<<endl;
	s.call(ctx);
	FunctionT f;
	f = std::move(s.f);
	cout<<"s.f valid = "<<(bool)s.f<<endl;
	f(ctx);
	f = 0;
	cout<<"someother1"<<endl;
	//f = std::move(FunctionT(Test("someotherthing", 3), boost::pool_allocator<Test>()));
	f.assign(Test("", 3), Allocator<Test>());
	f(ctx);
	f = 0;
	cout<<"someother2"<<endl;
	f.assign(Test("", 5), Allocator<Test>());
	f(ctx);
	return 0;
}

