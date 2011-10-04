#ifndef DL_TESA_HPP
#define DL_TESA_HPP


struct Test{
	virtual ~Test(){}
	virtual void print() = 0;
};

class TestA{
public:
	static TestA& instance();
	TestA();
	void set(int _a);
	int get()const;
private:
	int a;
};


#endif
