/* Implementation file sharedbackend.cpp
	
	Copyright 2012 Valentin Palade 
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

#include "system/sharedbackend.hpp"


struct SharedBackend::Data{
	
};

/*static*/ SharedBackend& SharedBackend::the(){
	static SharedBackend sb;
	return sb;
}
	
SharedStub* SharedBackend::create(void *_pv, SharedStub::DelFncT _cbk){
	return NULL;
}

void SharedBackend::use(SharedStub &_rss){
	
}
void SharedBackend::release(SharedStub &_rss){
	
}

SharedBackend::SharedBackend():d(*(new Data)){
	
}
