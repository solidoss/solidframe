#include <iostream>
#include <array>
#include "findmin.hpp"
using namespace std;


size_t ret_first(){
	return -1;
}

bool less_cmp(const size_t _v1, const size_t _v2){
	return _v1 < _v2;
}

int main(int argc, char *argv[]){
	
	array<int, 10>	arr1 = {112, 43, 74, 86, 101, 11};
	array<int, 10>	arr2 = {112, 112, 112, 112, 112, 112};
	
	cout<<find_min_or_equal(arr1.begin(), ret_first, NumberToType<6>())<<endl;
	cout<<find_min_or_equal(arr2.begin(), ret_first, NumberToType<6>())<<endl;
	cout<<find_min_or_equal_cmp(arr1.begin(), ret_first, less_cmp, NumberToType<6>())<<endl;
	cout<<find_min_or_equal_cmp(arr2.begin(), ret_first, less_cmp, NumberToType<6>())<<endl;
	
	cout<<find_min(arr1.begin(), NumberToType<6>())<<endl;
	cout<<find_min(arr2.begin(), NumberToType<6>())<<endl;
	cout<<find_min_cmp(arr1.begin(), less_cmp, NumberToType<6>())<<endl;
	cout<<find_min_cmp(arr2.begin(), less_cmp, NumberToType<6>())<<endl;
	return 0;
}