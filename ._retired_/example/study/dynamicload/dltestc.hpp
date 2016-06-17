#ifndef DL_TESTC_HPP
#define DL_TESTC_HPP

#include "dltesta.hpp"

class TestC: public Test{
public:
	TestC(int _c);
	~TestC();
	void set(int _c);
	int get()const;
	void print();
private:
	int c;
};


#endif
