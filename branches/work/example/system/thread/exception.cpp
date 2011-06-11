#include "system/exception.hpp"
#include <iostream>
#include "system/common.hpp"

using namespace std;


// template <
// 	class T0, class T1 = NullType, class T2 = NullType, class T3 = NullType, class T4 = NullType,
// 	class T5 = NullType, class T6 = NullType, class T7 = NullType, class T8 = NullType, class T9 = NullType
// >
// struct Tuple;
// 
// 
// template <class T0>
// struct Tuple<
// 	T0, NullType, NullType, NullType, NullType,
// 	 NullType, NullType, NullType, NullType, NullType
// >{
// 	Tuple(){}
// 	Tuple(const T0&_ro0):o0(_ro0){}
// 	T0	o0;
// 	std::ostream& print(std::ostream &_ros)const{
// 		_ros<<'['<<o0<<']'<<endl;
// 		return _ros;
// 	}
// };
// 
// template <class T0>
// Tuple<T0> make_tuple(const T0& _ro0){
// 	return Tuple<T0>(_ro0);
// }
// 
// template <class T0, class T1>
// struct Tuple<
// 	T0, T1, NullType, NullType, NullType,
// 	 NullType, NullType, NullType, NullType, NullType
// >{
// 	Tuple(){}
// 	Tuple(const T0&_ro0, const T1&_ro1):o0(_ro0), o1(_ro1){}
// 	T0	o0;
// 	T1	o1;
// 	std::ostream& print(std::ostream &_ros)const{
// 		_ros<<'['<<o0<<','<<o1<<']';
// 		return _ros;
// 	}
// };
// 
// template <class T0, class T1>
// Tuple<T0, T1> make_tuple(const T0& _ro0, const T1& _ro1){
// 	return Tuple<T0, T1>(_ro0, _ro1);
// }
// 
// 
// template <class T0, class T1, class T2>
// struct Tuple<
// 	T0, T1, T2, NullType, NullType,
// 	 NullType, NullType, NullType, NullType, NullType
// >{
// 	Tuple(){}
// 	Tuple(const T0&_ro0, const T1&_ro1, const T2&_ro2):o0(_ro0), o1(_ro1), o2(_ro2){}
// 	T0	o0;
// 	T1	o1;
// 	T2	o2;
// 	std::ostream& print(std::ostream &_ros)const{
// 		_ros<<'['<<o0<<','<<o1<<','<<o2<<']';
// 		return _ros;
// 	}
// };
// 
// template <class T0, class T1, class T2>
// Tuple<T0, T1, T2> make_tuple(const T0& _ro0, const T1& _ro1, const T2& _ro2){
// 	return Tuple<T0, T1, T2>(_ro0, _ro1, _ro2);
// }

struct TwoInts{
	TwoInts(int _a, int _b):a(_a), b(_b){}
	int a;
	int b;
};

std::ostream& operator<<(std::ostream &_ros, const TwoInts &_r2i){
	_ros<<'('<<_r2i.a<<','<<_r2i.b<<')';
	return _ros;
}

// template <
// 	typename T0, typename T1, typename T2, typename T3, typename T4,
// 	typename T5, typename T6, typename T7, typename T8, typename T9
// >
// std::ostream& operator<<(
// 	std::ostream &_ros,
// 	const Tuple<T0, T1, T2, T3, T4, T5, T6, T7, T8, T9> &_rt){
// 	return _rt.print(_ros);
// }

int main(){
#ifdef UDEBUG
	Dbg::instance().initStdErr();
#endif
	try{
		THROW_EXCEPTION("simple exception ever");
		//create_exeption("best exception ever", __FILE__, __LINE__);
		//throw Exception("best exception ever", __FILE__, __LINE__);
	}catch(exception& e){
		cout<<"caught exeption: "<<e.what()<<endl;
	}
	
	try{
		THROW_EXCEPTION_EX("3-tuple exception", make_tuple(2,TwoInts(9,10),static_cast<const char*>("a string")));
	}catch(exception& e){
		cout<<"caught exeption: "<<e.what()<<endl;
	}
	
	return 0;
}

