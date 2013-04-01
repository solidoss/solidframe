/* Declarations file consensusregistrar.hpp
	
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

#ifndef SOLID_CONSENSUS_REGISTRAR_HPP
#define SOLID_CONSENSUS_REGISTRAR_HPP

#include "frame/common.hpp"

namespace solid{
namespace consensus{

class Registrar{
public:
	static Registrar& the();
	frame::IndexT  registerObject(const frame::ObjectUidT &_robjuid, const frame::IndexT &_ridx = INVALID_INDEX);
	frame::ObjectUidT objectUid(const frame::IndexT &_ridx)const;
private:
	Registrar();
	~Registrar();
	Registrar(const Registrar&);
	Registrar& operator=(const Registrar&);
private:
	struct Data;
	Data &d;
};


}//namespace consensus
}//namespace solid

#endif
