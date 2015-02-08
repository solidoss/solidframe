#include <iostream>
#include <deque>
#include "system/common.hpp"
#include "system/atomic.hpp"
#include "system/mutex.hpp"

using namespace std;
using namespace solid;

typedef std::pair<size_t, uint32>	UniqueIdT;


class ObjectBase{
public:
	ObjectBase(size_t _v):v(_v){}
	
	void print()const{
		cout<<"Object value = "<<v<<endl;
	}
private:
	size_t v;
};

class Manager;

class Service{
public:
	Service(Manager &_rm);
	~Service();
	
	UniqueIdT registerObject(ObjectBase *_pobj);
	
	template <class F>
	void forEach(F _fnc){
		
	}
	
	void stop(bool _wait = true);
};


class Manager{
public:
	Manager();
	~Manager();
	
	template <class F>
	void forOne(UniqueIdT const &_ruid, F _fnc){
		
	}
	
	void stop(bool _wait = true);
	
private:
};

//-----------------------------------------------------------------------------
Service::Service(Manager &_rm){}
Service::~Service(){}
UniqueIdT Service::registerObject(ObjectBase *_pobj){
	return UniqueIdT(0, 0);
}
void Service::stop(bool _wait/* = true*/){
	
}
//-----------------------------------------------------------------------------
Manager::Manager(){}
Manager::~Manager(){}
UniqueIdT Manager::registerObject(ObjectBase *_pobj){
	return UniqueIdT(0, 0);
}
void Manager::stop(bool _wait/* = true*/){
	
}
//-----------------------------------------------------------------------------
void object_print(ObjectBase const *_pobj){
	if(_pobj){
		_pobj->print();
	}else{
		cout<<"NULL pointer"<<endl;
	}
}
//-----------------------------------------------------------------------------
typedef std::deque<UniqueIdT>	UniqueIdDequeT;

int main(int argc, char *argv[]){
	
	Manager				m;
	
	Service				s0(m);
	Service				s1(m);
	Service				s2(m);
	
	UniqueIdDequeT		udq0;
	UniqueIdDequeT		udq1;
	UniqueIdDequeT		udq2;
	
	for(size_t i = 0; i < 9; ++i){
		if((i % 3) == 0){
			udq0.push_back(m.registerObject(new ObjectBase(i)));
		}else if((i % 3) == 1){
			udq1.push_back(s1.registerObject(new ObjectBase(i)));
		}else if((i % 3) == 2){
			udq2.push_back(s2.registerObject(new ObjectBase(i)));
		}
	}
	cout<<"Sizeof(Mutex): "<<sizeof(Mutex)<<endl;
	cout<<"Manager:"<<endl;
	s0.forEach(object_print);
	
	for(auto it = udq0.begin(); it != udq0.end(); ++it){
		m.forOne(*it, object_print);
	}
	
	cout<<"Service1:"<<endl;
	s1.forEach(object_print);
	
	for(auto it = udq1.begin(); it != udq1.end(); ++it){
		m.forOne(*it, object_print);
	}
	
	cout<<"Service2:"<<endl;
	s2.forEach(object_print);
	
	for(auto it = udq2.begin(); it != udq2.end(); ++it){
		m.forOne(*it, object_print);
	}
	return 0;
}

