#include "cliparser.hpp"
#include <ctype.h>
#include <cstdio>
#include "utility/common.hpp"

template <typename T, T v1, T v2>
T* find_any(T *_t){
    while(*_t != v1 && *_t != v2){
        ++_t;
    }
    return _t;
}

template <typename T, T v1, T v2, T v3>
T* find_any(T *_t){
    while(*_t != v1 && *_t != v2 && *_t != v3){
        ++_t;
    }
    return _t;
}

template <typename T, T v1, T v2, T v3, T v4>
T* find_any(T *_t){
    while(*_t != v1 && *_t != v2 && *_t != v3 && *_t != v4){
        ++_t;
    }
    return _t;
}

template <typename T, T v1, T v2, T v3, T v4, T v5>
T* find_any(T *_t){
    while(*_t != v1 && *_t != v2 && *_t != v3 && *_t != v4 && *_t != v5){
        ++_t;
    }
    return _t;
}

namespace cli{
Parser::Parser(const char *_pb):pb(_pb){}
    
bool Parser::isAtEnd()const{
    return !pb || (*pb == 0) || (*pb == '\r') || (*pb == '\n');
}

void Parser::skipWhites(){
    while(!isAtEnd() && isspace(*pb)){
        ++pb;
    }
}

bool Parser::parse(int &_v){
    const char *p = find_any<const char, ' ', '\t', '\r', '\n', '\0'>(pb);
    sscanf(pb, "%d", &_v);
    bool r  = (p != pb);
    pb = p;
    return r;
}
bool Parser::parse(std::string &_v){
    const char *p = find_any<const char, ' ', '\t', '\r', '\n', '\0'>(pb);
    _v.assign(pb, p - pb);
    bool r  = (p != pb);
    pb = p;
    return r;
}
bool Parser::parse(char &_c){
    if(!isAtEnd() && !isspace(*pb)){
        _c = *pb;
        ++pb;
        return true;
    }
    return false;
}
}