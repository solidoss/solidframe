/* Implementation file service.cpp
	
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

#include <deque>
#include <vector>
#include <algorithm>
#include <cstring>

#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/exception.hpp"

#include "system/mutualstore.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"

#include "frame/service.hpp"
#include "frame/object.hpp"
#include "frame/manager.hpp"
#include "frame/message.hpp"
#include "frame/common.hpp"

namespace solid{
namespace frame{

}//namespace frame
}//namespace solid


