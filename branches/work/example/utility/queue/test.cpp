// test.cpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "utility/queue.hpp"
#include <iostream>

using namespace std;
using namespace solid;

int main(){
	cout<<"This is a queue test program..."<<endl;
	{
		Queue<ulong,2> q;
		q.push(1);
	}
	{
		Queue<ulong,2> q;
		
		for(int i = 0; i<10; ++i){
			q.push(i);
			cout<<"front = "<<q.front()<<endl;
			cout<<"back = "<<q.back()<<endl;
			q.pop();
		}
		
		cout<<"size = "<<q.size()<<endl;
		
		for(int i = 0; i<10; ++i){
			q.push(i);
		}
		while(!q.empty()){
			cout<<"front = "<<q.front()<<" back = "<<q.back()<<endl;
			q.pop();
		}
		cout<<"done clear queue"<<endl;
		
		q.push(2);
		q.push(3);
		q.push(4);
		
		cout<<"front = "<<q.front()<<endl;
		q.pop();
		
		cout<<"front = "<<q.front()<<endl;
		q.pop();
		
		q.push(5);
		q.push(6);
		q.push(7);
		while(!q.empty()){
			cout<<"front = "<<q.front()<<endl;
			q.pop();
		}
	}
	return 0;
}

