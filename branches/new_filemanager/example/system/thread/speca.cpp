#include "speca.hpp"
#include <vector>
#include <string>
#include "system/specific.hpp"
#include "system/debug.hpp"
#include <iostream>
#include "system/cassert.hpp"

typedef Cacheable<std::vector<int>, 2> CacheableVecT;
typedef Cacheable<std::string, 2> CacheableStringT;

void testa(){
	CacheableVecT *pcv = new CacheableVecT;
	CacheableStringT *pcs = new CacheableStringT;
	Specific::tryUncache<CacheableVecT>();
	Specific::tryUncache<CacheableStringT>();
	idbg("cache vector "<<(void*)pcv);
	std::cout<<"cache vector "<<(void*)pcv<<std::endl;
	Specific::cache(pcv);
	pcs->assign("gigi duru");
	idbg("caching string "<<(void*)pcs);
	std::cout<<"caching string "<<(void*)pcs<<std::endl;
	Specific::cache(pcs);
	
}

