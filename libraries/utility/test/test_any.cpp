#include "utility/any.hpp"
#include "system/cassert.hpp"
#include <string>
#include <iostream>
#include "system/exception.hpp"

#include "boost/any.hpp"

using namespace solid;
using namespace std;

//#define TEST_BOOST_ANY


struct TestNoCopy{
	std::string		str;
	
	TestNoCopy(const std::string &_rstr):str(_rstr){}
	
	TestNoCopy(const TestNoCopy&) = delete;
	TestNoCopy& operator=(const TestNoCopy&) = delete;
	
	TestNoCopy(TestNoCopy &&_utnc):str(std::move(_utnc.str)){}
	
	TestNoCopy& operator=(TestNoCopy &&_utnc){
		str = std::move(_utnc.str);
		return *this;
	}
};

#ifdef TEST_BOOST_ANY
std::string test_boost_any(const boost::any &_rany){
	boost::any		any0(_rany);
	std::string		retval;
	TestNoCopy		*p = boost::any_cast<TestNoCopy*>(any0);
	if(p){
		retval = p->str;
	}
	return retval;
}
#endif


void test_any_no_copy_copy(const Any<32> &_rany){
	try{
		Any<32>		tmp_any(_rany);
		SOLID_ASSERT(tmp_any.empty());
	}catch(std::exception& rex){
		cout<<"Exception: "<<rex.what()<<endl;
	}
	SOLID_ASSERT(not _rany.empty());
	
	Any<32>		tmp_any;
	
	try{
		tmp_any = _rany;
	}catch(std::exception& rex){
		cout<<"Exception: "<<rex.what()<<endl;
	}
	
	SOLID_ASSERT(tmp_any.empty());
	SOLID_ASSERT(not _rany.empty());
}

std::string test_any_no_copy_move(Any<32> &_rany){
	Any<32>		tmp_any(std::move(_rany));
	
	SOLID_ASSERT(_rany.empty());
	SOLID_ASSERT(!tmp_any.empty());
	
	TestNoCopy	*p = tmp_any.cast<TestNoCopy>();
	
	SOLID_ASSERT(p != nullptr);
	return p->str;
}

int test_any(int argc, char *argv[]){
#ifdef TEST_BOOST_ANY
	{
		boost::any		any0(TestNoCopy("a string"));
		
		TestNoCopy		*p = boost::any_cast<TestNoCopy*>(any0);
		if(p){
			cout<<"p->str = "<<p->str<<endl;
		}else{
			cout<<"p == nullptr"<<endl;
		}
		cout<<"copy string = "<<test_boost_any(any0)<<endl;
	}
	
#endif
	Any<>		any0;
	Any<32>		any32(any_create<32>(string("best string ever")));
	
	cout<<"sizeof(any0) = "<<sizeof(any0)<<endl;
	
	SOLID_ASSERT(not any32.empty());
	SOLID_ASSERT(any32.cast<string>() != nullptr);
	SOLID_ASSERT(any32.cast<int>() == nullptr);
	
	cout<<"value = "<<*any32.cast<string>()<<endl;
	
	
	any0 = std::move(any32);
	
	SOLID_ASSERT(any32.empty());
	SOLID_ASSERT(not any0.empty());
	
	SOLID_ASSERT(any0.cast<string>() != nullptr);
	SOLID_ASSERT(any0.cast<int>() == nullptr);
	
	cout<<"value = "<<*any0.cast<string>()<<endl;
	
	any32 = any0;
	
	SOLID_ASSERT(not any32.empty());
	SOLID_ASSERT(not any0.empty());
	
	Any<16>		any16_0(any32);
	Any<16>		any16_1(any16_0);
	
	SOLID_ASSERT(not any32.empty());
	SOLID_ASSERT(not any16_0.empty());
	SOLID_ASSERT(not any16_1.empty());
	
	SOLID_ASSERT(*any16_1.cast<string>() == *any32.cast<string>() && *any16_1.cast<string>() == *any16_0.cast<string>());
	
	Any<16>		any16_2(std::move(any16_0));
	
	SOLID_ASSERT(any16_0.empty());
	SOLID_ASSERT(not any16_2.empty());
	
	
	SOLID_ASSERT(*any16_2.cast<string>() == *any32.cast<string>());
	
	Any<32>		any_nc_0(any_create<32>(TestNoCopy("a string")));
	
	test_any_no_copy_copy(any_nc_0);
	
	cout<<"test_any_no_copy_move: "<<test_any_no_copy_move(any_nc_0)<<endl;
	
	return 0;
}