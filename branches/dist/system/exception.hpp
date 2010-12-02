#ifndef SYSTEM_EXCEPTION_HPP
#define SYSTEM_EXCEPTION_HPP

#include <exception>
#include <iosfwd>
#include <string>
#include <sstream>
#include "system/common.hpp"
#include "system/debug.hpp"

template <class T>
class Exception;
template <>
struct Exception<const char*>;

template <>
struct Exception<const char*>:std::exception{
	explicit Exception(
		const char *_pt,
		const char *_file,
		const int _line,
		const char *_func
	)throw(): t(_pt), file(_file), line(_line), function(_func){
		pdbg(file, line, function, "EXCEPTION: "<<t);
	}
	Exception(
		const Exception<const char*> &_rex
	)throw(): t(_rex.t), file(_rex.file), line(_rex.line),function(_rex.function){
	}
	
	~Exception()throw(){}
	Exception & operator=(Exception<const char*>& _rex)throw(){
		t = _rex.t;
		file = _rex.file;
		line =_rex.line;
		return *this;
	}
	const char* what()const throw(){
		if(w.empty()){
			std::ostringstream oss;
			oss<<'['<<src_file_name(file)<<':'<<line<<':'<<function<<']'<<':'<<' '<<t;
			w = oss.str();
		}
		return w.c_str();
	}
	
	const char			*t;
	const char			*file;
	int					line;
	const char			*function;
	mutable std::string	w;
};

template <class T>
struct Exception:std::exception{
	explicit Exception(
		const char *_pt,
		T const & _rt,
		const char *_file,
		const int _line,
		const char *_func
	)throw(): pt(_pt), t(_rt), file(_file), line(_line), function(_func){
		pdbg(file, line, function, "EXCEPTION: "<<pt<<':'<<' '<<t);
	}
	Exception(Exception<T> const &_rex)throw(): t(_rex.t), file(_rex.file), line(_rex.line){}
	
	~Exception()throw(){}
	Exception & operator=(Exception<T> const& _rex)throw(){
		t = _rex.t;
		file = _rex.file;
		line =_rex.line;
	}
	const char* what()const throw(){
		if(w.empty()){
			std::ostringstream oss;
			oss<<'['<<src_file_name(file)<<':'<<line<<':'<<function<<']'<<':'<<' '<<pt<<':'<<' '<<t;
			w = oss.str();
		}
		return w.c_str();
	}
	const char 			*pt;
	T					t;
	const char			*file;
	const int			line;
	const char			*function;
	mutable std::string	w;
};

void throw_exception(const char* const _pt, const char * const _file, const int _line, const char * const _func);

template <typename T>
void throw_exception(const char* const _pt, const T& _rt, const char *const _file, const int _line, const char * const _func){
	throw Exception<T>(_pt, _rt, _file, _line, _func);
}

#define CRT_FUNCTION_NAME __FUNCTION__

#define THROW_EXCEPTION_EX(x, y)\
	throw_exception(static_cast<const char*>(x), y, __FILE__, __LINE__, CRT_FUNCTION_NAME);

#define THROW_EXCEPTION(x)\
	throw_exception(static_cast<const char*>(x), __FILE__, __LINE__, CRT_FUNCTION_NAME);


#endif
