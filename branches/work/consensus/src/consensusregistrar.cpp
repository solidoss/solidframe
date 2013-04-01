/* Implementation file consensusregistrar.cpp
	
	Copyright 2013 Valentin Palade 
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

#include "consensus/consensusregistrar.hpp"


namespace solid{
namespace consensus{

struct Registrar::Data{
	
};

/*static*/ Registrar& Registrar::the(){
	static Registrar r;
	return r;
}

frame::IndexT  Registrar::registerObject(const frame::ObjectUidT &_robjuid, const frame::IndexT &_ridx){
	
}

frame::ObjectUidT Registrar::objectUid(const frame::IndexT &_ridx)const{
	
}

Registrar::Registrar():d(*(new Data)){}

Registrar::~Registrar(){
	delete &d;
}

}//namespace consensus
}//namespace solid
