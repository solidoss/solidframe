/* Implementation file testc.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidFrame framework.

	SolidFrame is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	SolidFrame is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidFrame.  If not, see <http://www.gnu.org/licenses/>.
*/

//g++ -o testc testc.cpp threadpp.cpp -lpthread -lrt
#include <iostream>
#include "../debug.h"
#include "threadpp.h"
#include "synchropp.h"
#include "../utils.h"
using namespace std;


int main(){
	Mutex 		m;
	Condition	c;
	timespec ts;
	m.lock();
	cout<<"before enter"<<endl;
	clock_gettime(CLOCK_REALTIME, &ts);
	cout<<"tv_sec = "<<ts.tv_sec<<" ts.tv_nsec = "<<ts.tv_nsec<<endl;
	cout<<"time(null) = "<<time(NULL)<<endl;
	ts.tv_sec += 5;
	ts.tv_nsec += 100;
	c.wait(m, ts);
	cout<<"after exit"<<endl;
	m.unlock();
	return 0;
}

