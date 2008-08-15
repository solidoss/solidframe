/* Implementation file test.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidGround is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "utility/queue.hpp"
#include <iostream>

using namespace std;

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

