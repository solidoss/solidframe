#include "specb.hpp"
#include <string>
#include "system/specific.hpp"
#include "system/debug.hpp"
#include <iostream>

typedef Cacheable<std::string, 2> CacheableStringT;

void testb(){
	CacheableStringT *pcs = Specific::uncache<CacheableStringT>();
	idbg("uncached string (it should be the same pointer as the above cached one) "<<(void*)pcs);
	std::cout<<"uncached string (it should be the same pointer as the above cached one) "<<(void*)pcs<<std::endl;
	Specific::cache(pcs);
}

