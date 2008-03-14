/* Declarations file idtypemap.hpp
	
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

#ifndef ID_TYPE_MAP_HPP
#define ID_TYPE_MAP_HPP

#include <string>

#include "system/debug.hpp"
#include "system/common.hpp"

#include "basetypemap.hpp"

namespace serialization{

class IdTypeMap: public BaseTypeMap{
public:
	IdTypeMap();
	~IdTypeMap();
	//its a dummy implementation
	static IdTypeMap* the(){
		return NULL;
	}
	/*virtual*/ void insert(FncTp, unsigned _pos, const char *, unsigned _maxpos);
	template <class Ser>
	FncTp storeTypeId(Ser &_rs, const char *_name, std::string &_rstr, ulong _serid){
		FncTp pf;
		getFunction(pf, _name, _rstr, _serid) & _rs;
		return pf;
	}
	template <class Des>
	void parseTypeIdPrepare(Des &_rd, std::string &_rstr){
		idbg("");
	}
	FncTp parseTypeIdDone(const std::string &_rstr, ulong _serid);
private:
	ulong & getFunction(FncTp &_rpf, const char *_name, std::string &_rstr, ulong _serid);
	struct Data;
	Data	&d;
};

}

#endif
