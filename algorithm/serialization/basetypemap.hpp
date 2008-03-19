/* Declarations file basetypemap.hpp
	
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
#ifndef BASE_TYPE_MAP_HPP
#define BASE_TYPE_MAP_HPP

namespace serialization{

class BaseTypeMap{
public:
	typedef void (*FncTp)(void*, void*, void*);
	virtual void insert(FncTp, unsigned _pos, const char *_name, unsigned _maxpos) = 0;
	virtual ~BaseTypeMap();
};

}

#endif
