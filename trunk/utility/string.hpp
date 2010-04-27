#ifndef UTILITY_STRING_HPP
#define UTILITY_STRING_HPP

#include "system/common.hpp"

const char * charToString(unsigned _c);

//! Some cross platform cstring utility functions
struct cstring{
    //! Equivalent to strcmp
    static int cmp(const char* _s1, const char *_s2);
    //! Equivalent to strncmp
    static int ncmp(const char* _s1, const char *_s2, uint _len);
    //! Equivalent to strcasecmp
    static int casecmp(const char* _s1, const char *_s2);
    //! Equivalent to strncasecmp
    static int ncasecmp(const char* _s1, const char *_s2, uint _len);
    template <class T, T C1>
	static T* find(T *_pc){
		while(*_pc != C1) ++_pc;
		return _pc;
	}
	
	template <class T, T C1, T C2>
	static T* find(T *_pc){
		while(*_pc != C2 && *_pc != C1) ++_pc;
		return _pc;
	}
	
	template <class T, T C1, T C2, T C3>
	static T* find(T *_pc){
		while(*_pc != C2 && *_pc != C1 && *_pc != C3) ++_pc;
		return _pc;
	}
	template <class T, T C1, T C2, T C3>
	static T* findNot(T *_pc){
		while(*_pc == C2 || *_pc == C1 || *_pc == C3) ++_pc;
		return _pc;
}
};

#endif
