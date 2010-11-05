/* Declarations file basetypemap.hpp
	
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
#ifndef ALGORITHM_SERIALIZATION_BASE_TYPE_MAP_HPP
#define ALGORITHM_SERIALIZATION_BASE_TYPE_MAP_HPP

namespace serialization{
//! A base class for mapping types to callbacks
class BaseTypeMap{
public:
	typedef void (*FncT)(void*, void*, void*);
	//! Insert a function callback
	virtual void insert(FncT, unsigned _pos, const char *_name, unsigned _maxpos) = 0;
	virtual ~BaseTypeMap();
};

}

#endif
