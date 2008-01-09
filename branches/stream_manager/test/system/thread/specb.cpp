#include "specb.h"
#include <string>
#include "system/specific.h"
#include "system/debug.h"
#include <iostream>

typedef Cacheable<std::string, 2> CacheableStringTp;

void testb(){
	CacheableStringTp *pcs = Specific::uncache<CacheableStringTp>();
	idbg("uncached string (it should be the same pointer as the above cached one) "<<(void*)pcs);
	std::cout<<"uncached string (it should be the same pointer as the above cached one) "<<(void*)pcs<<std::endl;
	Specific::cache(pcs);
}

