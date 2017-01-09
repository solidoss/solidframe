#include "system/function.hpp"
#include <iostream>
using namespace std;


typedef FUNCTION<int(std::string const&)>   FunctionT;

void* operator new(size_t sz) throw (std::bad_alloc)
{

    void* mem = malloc(sz);
    if (mem){
        cerr << "allocating " << sz << " bytes "<<mem<<endl;
        return mem;
    }else
        throw std::bad_alloc();
}


void operator delete(void* ptr) throw()
{
    cerr << "deallocating at " << ptr << endl;
    free(ptr);
}

class Test{
public:
    Test(const char * _str = ""):s(_str){}
    void setFnc(FunctionT &_fnc){
        _fnc = [this](std::string const& _str){
            this->s = _str;
            return _str.size();
        };
    }
private:
    std::string s;
};

std::string     test_str;

static int test_fnc(std::string const &_str){
    test_str = _str;
    return _str.size();
}

struct Functor{
    char            buf[8];
    std::string     str;
    Functor(){}

    int operator()(std::string const &_str){
        str = _str;
        return _str.size();
    }
};

int main(int argc, char *argv[]){

    Test            test("fool");

    cout<<"Create function:"<<endl;

    FunctionT       fnc;

    cout<<"sizeof(fnc): "<<sizeof(fnc)<<endl;

    cout<<"Init with lamda:"<<endl;
    fnc = [](std::string const &_str){test_str = _str; return static_cast<int>(_str.size());};

    cout<<"Init with static:"<<endl;
    fnc = test_fnc;

    cout<<"Init with test:"<<endl;
    test.setFnc(fnc);

    cout<<"Init with functor (sizeof(Functor) = "<<sizeof(Functor)<<"):"<<endl;
    fnc = Functor();
    return 0;
}

