#ifndef UTILITY_MEMORY_HPP
#define UTILITY_MEMORY_HPP

struct EmptyChecker{
	EmptyChecker(const char *_fncname):v(0), fncname(_fncname){}
	~EmptyChecker();
	void add();
	void sub();
private:
	unsigned long v;
	const char * fncname;
};

#ifdef UDEBUG
template <class T>
void objectCheck(bool _add, const char *_fncname){
	static EmptyChecker ec(_fncname);
	if(_add)	ec.add();
	else 		ec.sub();
}
#else
template <class T>
void objectCheck(bool _add, const char *){
}
#endif


#endif
