// solid/system/exception.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SYSTEM_EXCEPTION_HPP
#define SYSTEM_EXCEPTION_HPP

#include <exception>
#include <iosfwd>
#include <string>
#include <sstream>
#include "solid/system/common.hpp"
#include "solid/system/debug.hpp"
#include "solid/system/error.hpp"

namespace solid{

template <class T>
struct Exception;
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
            oss<<'['<<src_file_name(file)<<'('<<line<<')'<<' '<<function<<']'<<':'<<' '<<t;
            w = oss.str();
        }
        return w.c_str();
    }
    
    const char          *t;
    const char          *file;
    int                 line;
    const char          *function;
    mutable std::string w;
};

template <>
struct Exception<ErrorCodeT>:std::exception{
    explicit Exception(
        const char *_pt,
        ErrorCodeT const & _rt,
        const char *_file,
        const int _line,
        const char *_func
    )throw(): pt(_pt), t(_rt), file(_file), line(_line), function(_func){
        pdbg(file, line, function, "EXCEPTION: "<<pt<<' '<<t<<" - "<<t.message());
    }
    Exception(
        const Exception<ErrorCodeT> &_rex
    )throw(): pt(_rex.pt), t(_rex.t), file(_rex.file), line(_rex.line),function(_rex.function){
    }
    
    ~Exception()throw(){}
    Exception & operator=(Exception<ErrorCodeT> const & _rex)throw(){
        pt= _rex.pt;
        t = _rex.t;
        file = _rex.file;
        line =_rex.line;
        return *this;
    }
    const char* what()const throw(){
        if(w.empty()){
            std::ostringstream oss;
            oss<<'['<<src_file_name(file)<<'('<<line<<')'<<' '<<function<<"]: "<<pt<<' '<<t<<" - "<<t.message();
            w = oss.str();
        }
        return w.c_str();
    }
    
    const char              *pt;
    ErrorCodeT              t;
    const char              *file;
    int                     line;
    const char              *function;
    mutable std::string     w;
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
            oss<<'['<<src_file_name(file)<<'('<<line<<')'<<' '<<function<<']'<<':'<<' '<<pt<<':'<<' '<<t;
            w = oss.str();
        }
        return w.c_str();
    }
    const char          *pt;
    T                   t;
    const char          *file;
    const int           line;
    const char          *function;
    mutable std::string w;
};

inline void throw_exception(const char* const _pt, const char * const _file, const int _line, const char * const _func){
    throw Exception<const char*>(_pt, _file, _line, _func);
}

template <typename T>
void throw_exception(const char* const _pt, const T& _rt, const char *const _file, const int _line, const char * const _func){
    throw Exception<T>(_pt, _rt, _file, _line, _func);
}


#ifndef CRT_FUNCTION_NAME
    #ifdef SOLID_ON_WINDOWS
        #define CRT_FUNCTION_NAME __func__
    #else
        #define CRT_FUNCTION_NAME __FUNCTION__
    #endif
#endif


#define SOLID_THROW_EX(x, y)\
    throw Exception<decltype(y)>(static_cast<const char*>(x), y, __FILE__, __LINE__, CRT_FUNCTION_NAME);

#define SOLID_THROW(x)\
    throw Exception<const char*>(static_cast<const char*>((x)), __FILE__, __LINE__, CRT_FUNCTION_NAME);

    
#define SOLID_CHECK(a)\
    if(!(a)) SOLID_THROW("Failed checking: "#a);

#define SOLID_CHECK_ERROR(err)\
    if((err)) SOLID_THROW_EX("Failed checking ["#err"]", (err).message());

}//namespace solid

#endif
