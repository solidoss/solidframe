#include <unordered_map>
#include <utility>
#include <cstddef>

typedef std::pair<unsigned short, unsigned>									UnsignedPairT;

struct PairCmpEqual{
	bool operator()(const UnsignedPairT& _req1, const UnsignedPairT& _req2)const{
		return _req1.first == _req2.first && _req1.second == _req2.second;
	}
};

struct PairHash{
	size_t operator()(const UnsignedPairT & _req1)const{
		return _req1.second ^ _req1.first;
	}
};

typedef std::unordered_map<UnsignedPairT, unsigned, PairHash, PairCmpEqual>	PairMapT;

int main(){
	PairMapT	pm;
	return 0;
}