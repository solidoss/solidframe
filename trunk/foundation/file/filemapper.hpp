/* Declarations file filemapper.hpp
	
	Copyright 2010 Valentin Palade 
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

#ifndef FILE_MAPPER_HPP
#define FILE_MAPPER_HPP

#include "foundation/file/filemanager.hpp"

struct TimeSpec;

namespace foundation{
namespace file{

struct Mapper{
	virtual ~Mapper();
	virtual void id(uint32 _id) = 0;
	virtual void execute(Manager::Stub &_rs) = 0;
	virtual void stop() = 0;
	virtual bool erase(File *_pf) = 0;
	virtual File* findOrCreateFile(const Key &_rk) = 0;
	virtual int open(File &_rf) = 0;
	virtual void close(File &_rf) = 0;
	virtual bool setTimeout(TimeSpec &_rts) = 0;
};

}//namespace file
}//namespace foundation

#endif