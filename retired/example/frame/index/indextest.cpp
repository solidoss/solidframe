#include "frame/common.hpp"
#include "system/cassert.hpp"
#include <bitset>

#include <iostream>
using namespace std;
using namespace solid;
using namespace solid::frame;


int main(int argc, char *argv[]){
	
	typedef bitset<IndexBitCount> IndexBitSetT;
	
	IndexT 			h(1), l(2);
	IndexT			uv = unite_index(h, l, 1);
	IndexBitSetT	uvbs(uv);
	
	IndexT			hv,lv;
	
	split_index(hv, lv, 1, uv);
	
	IndexBitSetT	hvbs(hv), lvbs(lv);
	
	cout<<uvbs<<" -> "<<hvbs<<" - "<<lvbs<<endl;
	cassert(hv == h && lv == l);
	
	h = 7;
	l = (1ULL << (IndexBitCount - 3)) - 1;
	
	uv = unite_index(h, l, 3);
	split_index(hv, lv, 3, uv);
	uvbs = uv;
	hvbs = hv;
	lvbs = lv;
	cout<<uvbs<<" -> "<<hvbs<<" - "<<lvbs<<endl;
	cassert(hv == h && lv == l);
	
	for(int i = 1; i <= 16; ++i){
		IndexT maxhicnt = (1 << i) - 1;
		IndexT maxlocnt = (1 << (IndexBitCount - i)) - 1;
		if(maxlocnt > 0xffffff) maxlocnt = 0xffffff;
		for(IndexT h = 0; h < maxhicnt; ++h){
			for(IndexT l = 1; l < maxlocnt; l += 100){
				uv = unite_index(h, l, i);
				
				split_index(hv, lv, i, uv);
				cassert(hv == h && lv == l);
			}
		}
	}
	
	
	return 0;
}