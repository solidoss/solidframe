#include "speca.h"
#include <vector>
#include <string>
#include "system/specific.h"
#include "system/debug.h"
#include <iostream>
#include <cassert>

typedef Cacheable<std::vector<int>, 2> CacheableVecTp;
typedef Cacheable<std::string, 2> CacheableStringTp;

void testa(){
	CacheableVecTp *pcv = new CacheableVecTp;
	CacheableStringTp *pcs = new CacheableStringTp;
	assert(!Specific::tryUncache<CacheableVecTp>());
	assert(!Specific::tryUncache<CacheableStringTp>());
	idbg("cache vector "<<(void*)pcv);
	std::cout<<"cache vector "<<(void*)pcv<<std::endl;
	Specific::cache(pcv);
	pcs->assign("gigi duru");
	idbg("caching string "<<(void*)pcs);
	std::cout<<"caching string "<<(void*)pcs<<std::endl;
	Specific::cache(pcs);
	
}

