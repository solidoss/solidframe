#ifndef STUDY_FINDMIN_HPP
#define STUDY_FINDMIN_HPP

template <size_t S>
struct NumberToType{
	enum{
		value = S,
	};
};

typedef std::pair<size_t, bool>		FindRetValT;

template <class It>
FindRetValT find_min_or_equal(It _it, NumberToType<1> _s){
	return FindRetValT(0, true);
}

template <class It>
FindRetValT find_min_or_equal(It _it, NumberToType<2> _s){
	if(*_it < *(_it + 1)){
		return FindRetValT(0, false);
	} else if(*_it > *(_it + 1)){
		return FindRetValT(1, false);
	}
	return FindRetValT(0, true);
}

template <class It, size_t S>
FindRetValT find_min_or_equal(It _it, NumberToType<S> s){
	
	const FindRetValT	off1 = find_min_or_equal(_it, NumberToType<S/2>());
	FindRetValT 		off2 = find_min_or_equal(_it + S/2, NumberToType<S - S/2>());
	off2.first += S/2;
	
	if(*(_it + off1.first) < *(_it + off2.first)){
		return FindRetValT(off1.first, false);
	}else if(*(_it + off1.first) > *(_it + off2.first)){
		return FindRetValT(off2.first, false);
	}
	return FindRetValT(off1.first, off1.second && off2.second);
}

template <class It, class C, size_t S>
size_t find_min_or_equal(It _it, C _c, NumberToType<S> s){
	FindRetValT rv = find_min_or_equal(_it, s);
	if(!rv.second){
		return rv.first;
	}else{
		return _c();
	}
}


template <class It, class Cmp>
FindRetValT find_min_or_equal_cmp(It _it, Cmp const & _rcmp, NumberToType<1> _s){
	return FindRetValT(0, true);
}

template <class It, class Cmp>
FindRetValT find_min_or_equal_cmp(It _it, Cmp const & _rcmp, NumberToType<2> _s){
	if(_rcmp(*_it, *(_it + 1))){
		return FindRetValT(0, false);
	} else if(_rcmp(*(_it + 1), *_it)){
		return FindRetValT(1, false);
	}
	return FindRetValT(0, true);
}

template <class It, class Cmp, size_t S>
FindRetValT find_min_or_equal_cmp(It _it, Cmp const &_rcmp, NumberToType<S> s){
	
	const FindRetValT	off1 = find_min_or_equal_cmp(_it, _rcmp, NumberToType<S/2>());
	FindRetValT 		off2 = find_min_or_equal_cmp(_it + S/2, _rcmp, NumberToType<S - S/2>());
	off2.first += S/2;
	
	if(_rcmp(*(_it + off1.first), *(_it + off2.first))){
		return FindRetValT(off1.first, false);
	}else if(_rcmp(*(_it + off2.first), *(_it + off1.first))){
		return FindRetValT(off2.first, false);
	}
	return FindRetValT(off1.first, off1.second && off2.second);
}

template <class It, class C, class Cmp, size_t S>
size_t find_min_or_equal_cmp(It _it, C _c, Cmp const &_rcmp, NumberToType<S> s){
	FindRetValT rv = find_min_or_equal_cmp(_it, _rcmp, s);
	if(!rv.second){
		return rv.first;
	}else{
		return _c();
	}
}

template <class It>
size_t find_min(It _it, NumberToType<1> _s){
	return 0;
}

template <class It>
size_t find_min(It _it, NumberToType<2> _s){
	if(*_it < *(_it + 1)){
		return 0;
	}
	return 1;
}

template <class It, size_t S>
size_t find_min(It _it, NumberToType<S> s){
	
	const size_t	off1 = find_min(_it, NumberToType<S/2>());
	const size_t	off2 = find_min(_it + S/2, NumberToType<S - S/2>()) + S/2;
	
	if(*(_it + off1) < *(_it + off2)){
		return off1;
	}
	return off2;
}



template <class It, class Cmp>
size_t find_min_cmp(It _it, Cmp const &, NumberToType<1> _s){
	return 0;
}

template <class It, class Cmp>
size_t find_min_cmp(It _it, Cmp const &_rcmp, NumberToType<2> _s){
	if(_rcmp(*_it, *(_it + 1))){
		return 0;
	}
	return 1;
}

template <class It, class Cmp, size_t S>
size_t find_min_cmp(It _it, Cmp const &_rcmp, NumberToType<S> s){
	
	const size_t	off1 = find_min_cmp(_it, _rcmp, NumberToType<S/2>());
	const size_t	off2 = find_min_cmp(_it + S/2, _rcmp, NumberToType<S - S/2>()) + S/2;
	
	if(_rcmp(*(_it + off1), *(_it + off2))){
		return off1;
	}
	return off2;
}



#endif
