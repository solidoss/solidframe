// testbs.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/utility/algorithm.hpp"
#include <vector>
#include <iostream>
using namespace std;
using namespace solid;

std::ostream &operator<<(std::ostream &_ros, const binary_search_result_t &_r){
    if(_r.first){
        _ros<<"found: ";
    }else{
        _ros<<"not found: ";
    }
    _ros<<_r.second;
    return _ros;
}

int main(){
    vector<int> v;
    for(int i = 1; i < 11; ++i){
        v.push_back(i);
        v.push_back(i);
        v.push_back(i);
    }
    //BinarySeeker<> bs;
    const int *pd = v.data();
    int rv(0);
    cout<<"bs(10) = "<<v[rv]<<' '<<solid::binary_search(pd, pd + v.size(), static_cast<int>(10))<<endl;
    cout<<"bs(12) = "<<solid::binary_search(pd, pd + v.size(), static_cast<int>(12))<<endl;
    cout<<"bs(-1) = "<<solid::binary_search(pd, pd + v.size(), static_cast<int>(-1))<<endl;
    for(size_t i = 0; i < (v.size() + 2); ++i){
        cout<<"bs("<<i<<") = "<<solid::binary_search(pd, pd + v.size(), static_cast<int>(i))<<endl;
    }
    cout<<"bs(10) = "<<solid::binary_search(v.begin(), v.end(), static_cast<int>(10))<<endl;
    cout<<"bs(12) = "<<solid::binary_search(v.begin(), v.end(), static_cast<int>(12))<<endl;
    cout<<"bs(-1) = "<<solid::binary_search(v.begin(), v.end(), static_cast<int>(-1))<<endl;
    for(size_t i = 0; i < (v.size() + 2); ++i){
        cout<<"bs("<<i<<") = "<<solid::binary_search(v.begin(), v.end(), static_cast<int>(i))<<endl;
    }
    return 0;
}
