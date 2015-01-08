#include <iostream>
#include <array>
using namespace std;


template <size_t S>
struct EmptyType{
	enum{
		value = S,
	};
};

typedef std::pair<size_t, bool>		FindRetValT;

template <class It>
FindRetValT find_min_or_equal(It _it, EmptyType<1> _s){
	return FindRetValT(0, true);
}

template <class It>
FindRetValT find_min_or_equal(It _it, EmptyType<2> _s){
	if(*_it < *(_it + 1)){
		return FindRetValT(0, false);
	} else if(*_it > *(_it + 1)){
		return FindRetValT(1, false);
	}
	return FindRetValT(0, true);
}

template <class It, size_t S>
FindRetValT find_min_or_equal(It _it, EmptyType<S> s){
	
	const FindRetValT	off1 = find_min_or_equal(_it, EmptyType<S/2>());
	FindRetValT 		off2 = find_min_or_equal(_it + S/2, EmptyType<S - S/2>());
	off2.first += S/2;
	
	if(*(_it + off1.first) < *(_it + off2.first)){
		return FindRetValT(off1.first, false);
	}else if(*(_it + off1.first) > *(_it + off2.first)){
		return FindRetValT(off2.first, false);
	}
	return FindRetValT(off1.first, off1.second && off2.second);
}

template <class It, class C, size_t S>
size_t find_min(It _it, C _c, EmptyType<S> s){
	FindRetValT rv = find_min_or_equal(_it, s);
	if(!rv.second){
		return rv.first;
	}else{
		return _c();
	}
}


template <class It, class Less>
FindRetValT find_min_or_equal(It _it, Less const & _rless, EmptyType<1> _s){
	return FindRetValT(0, true);
}

template <class It, class Less>
FindRetValT find_min_or_equal(It _it, Less const & _rless, EmptyType<2> _s){
	if(_rless(*_it, *(_it + 1))){
		return FindRetValT(0, false);
	} else if(_rless(*(_it + 1), *_it)){
		return FindRetValT(1, false);
	}
	return FindRetValT(0, true);
}

template <class It, size_t S, class Less>
FindRetValT find_min_or_equal(It _it, Less const &_rless, EmptyType<S> s){
	
	const FindRetValT	off1 = find_min_or_equal(_it, _rless, EmptyType<S/2>());
	FindRetValT 		off2 = find_min_or_equal(_it + S/2, _rless, EmptyType<S - S/2>());
	off2.first += S/2;
	
	if(_rless(*(_it + off1.first), *(_it + off2.first))){
		return FindRetValT(off1.first, false);
	}else if(_rless(*(_it + off2.first), *(_it + off1.first))){
		return FindRetValT(off2.first, false);
	}
	return FindRetValT(off1.first, off1.second && off2.second);
}

template <class It, class C, class Less, size_t S>
size_t find_min(It _it, C _c, Less const &_rless, EmptyType<S> s){
	FindRetValT rv = find_min_or_equal(_it, _rless, s);
	if(!rv.second){
		return rv.first;
	}else{
		return _c();
	}
}

template <class It>
size_t find_min(It _it, EmptyType<1> _s){
	return 0;
}

template <class It>
size_t find_min(It _it, EmptyType<2> _s){
	if(*_it < *(_it + 1)){
		return 0;
	}
	return 1;
}

template <class It, size_t S>
size_t find_min(It _it, EmptyType<S> s){
	
	const size_t	off1 = find_min(_it, EmptyType<S/2>());
	const size_t	off2 = find_min(_it + S/2, EmptyType<S - S/2>()) + S/2;
	
	if(*(_it + off1) < *(_it + off2)){
		return off1;
	}
	return off2;
}



template <class It, class Less>
size_t find_min(It _it, Less const &_rless, EmptyType<1> _s){
	return 0;
}

template <class It, class Less>
size_t find_min(It _it, Less const &_rless, EmptyType<2> _s){
	if(_rless(*_it, *(_it + 1))){
		return 0;
	}
	return 1;
}

template <class It, class Less, size_t S>
size_t find_min(It _it, Less const &_rless, EmptyType<S> s){
	
	const size_t	off1 = find_min(_it, EmptyType<S/2>());
	const size_t	off2 = find_min(_it + S/2, EmptyType<S - S/2>()) + S/2;
	
	if(_rless(*(_it + off1), *(_it + off2))){
		return off1;
	}
	return off2;
}



size_t ret_first(){
	return -1;
}

bool less_cmp(const size_t _v1, const size_t _v2){
	return _v1 < _v2;
}

int main(int argc, char *argv[]){
	
	array<int, 10>	arr1 = {112, 43, 74, 86, 101, 11};
	array<int, 10>	arr2 = {112, 112, 112, 112, 112, 112};
	
	cout<<find_min(arr1.begin(), ret_first, EmptyType<6>())<<endl;
	cout<<find_min(arr2.begin(), ret_first, EmptyType<6>())<<endl;
	cout<<find_min(arr1.begin(), ret_first, less_cmp, EmptyType<6>())<<endl;
	cout<<find_min(arr2.begin(), ret_first, less_cmp, EmptyType<6>())<<endl;
	
	cout<<find_min(arr1.begin(), EmptyType<6>())<<endl;
	cout<<find_min(arr2.begin(), EmptyType<6>())<<endl;
	cout<<find_min(arr1.begin(), less_cmp, EmptyType<6>())<<endl;
	cout<<find_min(arr2.begin(), less_cmp, EmptyType<6>())<<endl;
	return 0;
}