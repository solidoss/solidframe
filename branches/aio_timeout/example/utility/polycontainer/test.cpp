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

#include <iostream>
#include <vector>
#include "polycontainerpp.h"

using namespace std;
///\cond 0
struct A{
	typedef int J;
	void set(int i){
		cout<<"A::set "<<i<<endl;
		a = i;
	}
	int a;
};

struct B{
	typedef string J;
	void set(string _s) { 
		cout<<"B::set "<<_s<<endl;
		s = _s;
	}
	string s;
};

struct C{
	typedef string J;
	void set(string _s){ 
		cout<<"C::set "<<_s<<endl;
		a = _s.size();
	}
	int a;
};


struct MyContainer: POLY2(A,C){
};

struct MyConstContainer: CONSTPOLY2(B*,std::vector<A*>){
	MyConstContainer(){
		set<B*>() = new B;
		set<std::vector<A*> >().push_back(new A);
	}
};

///\endcond

int main(){
	PolyHolder<int>		phi;
	PolyHolder<int*>	phpi;
	MyContainer			mc;
	MyConstContainer	mcc;
	//mc.get<A*>() = new A;
	mc.get<A>().set(10);
	mc.get<C>().set("gigi");
	
	mcc.get<B*>()->set("gigi");
	mcc.get<std::vector<A*> >()[0]->set(10);
	return 0;
}

