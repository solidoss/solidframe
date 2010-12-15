#ifndef CONCEPT_GAMMA_FILTERS_HPP
#define CONCEPT_GAMMA_FILTERS_HPP

namespace concept{
namespace gamma{

template <int C>
struct CharFilter{
	static bool check(int _c){
		return _c == C;
	}
};


template <typename T>
struct NotFilter{
	static bool check(int _c){
		return !T::check(_c);
	}
};

struct AtomFilter{
	static bool check(int _c){
		return isalpha(_c) || isdigit(_c);
	}
};

struct QuotedFilter{
    //TODO: change to use bitset
    static bool check(int _c){
    	if(!_c) 		return false;
        if(_c == '\r')	return false;
        if(_c == '\n')	return false;
        if(_c == '\"')	return false;
        if(_c == '\\')	return false;
        if(((unsigned char)_c) > 127) return false;
        return true;
    }
};

struct TagFilter{
	static bool check(int _c){
		return isalnum(_c);
	}
};

}//namespace gamma
}//namespace concept


#endif
