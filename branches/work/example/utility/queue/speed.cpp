// speed.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <iostream>
#include <queue>
#include <list>
#include "utility/queue.hpp"
using namespace std;
using namespace solid;

///\cond 0
struct D{
	D(int i):data(i){
		cout<<"D()"<<endl;
	}
	D():data(0){
		cout<<"D()"<<endl;
	}
	D(const D &_d):data(_d.data){
		cout<<"D(D)"<<endl;
	}
	D& operator=(const D &_d){
		data = _d.data;
		cout<<"operator ="<<endl;
	}
	~D(){
		cout<<"~D()"<<endl;
	}
	int data;
};
//typedef queue<int, list<int> > QueueT;
//typedef queue<int> QueueT;
typedef Queue<int>	QueueT;
///\endcond
int main(){
	ulong sum = 0;
	cout<<"sizeof queue<int> = "<<sizeof(QueueT)<<endl;
	QueueT q;
	for(int j = 0; j < 20; ++j){
		for(int i = 0; i < 100000; ++i){
			q.push(i);
			sum += q.front();
			q.pop();
		}
		
		while(q.size()){
			sum += q.front();
			q.pop();
		}//*/
	}
	cout<<"sum = "<<sum<<endl;
}

