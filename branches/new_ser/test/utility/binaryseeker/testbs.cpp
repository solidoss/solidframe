/* Implementation file testbs.cpp
	
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

#include "binaryseeker.h"
#include <vector>
#include <iostream>
using namespace std;

int main(){
	vector<int> v;
	for(int i = 1; i < 11; ++i){
		v.push_back(i);
	}
	BinarySeeker<> bs;
	cout<<"bs(10) = "<<bs(v.begin(), v.end(),10)<<endl;
	cout<<"bs(12) = "<<bs(v.begin(), v.end(),12)<<endl;
	cout<<"bs(-1) = "<<bs(v.begin(), v.end(),-1)<<endl;
	const int *pd = v.data();
	cout<<"bs(10) = "<<bs(pd, pd + v.size(), 10)<<endl;
	cout<<"bs(12) = "<<bs(pd, pd + v.size(), 12)<<endl;
	cout<<"bs(-1) = "<<bs(pd, pd + v.size(), -1)<<endl;
	
	return 0;
}
