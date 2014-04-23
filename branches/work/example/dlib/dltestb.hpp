#ifndef DL_TESTB_HPP
#define DL_TESTB_HPP

#include "dltesta.hpp"

class TestB: public Test{
public:
	TestB(int _b);
	~TestB();
	void set(int _b);
	int get()const;
	void print();
private:
	int b;
};


#endif
