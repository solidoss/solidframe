/* Declarations file readwriteservice.hpp
	
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

#ifndef FOUNDATION_READWRITESERVICE_HPP
#define FOUNDATION_READWRITESERVICE_HPP
#include <vector>
#include <stack>

#include "utility/mutualstore.hpp"

#include "foundation/service.hpp"

namespace foundation{
class ReadWriteService:public Service{
public:
	virtual ~ReadWriteService();
	Condition* popCondition(Object &_robj);
	void pushCondition(Object &_robj, Condition *&_rpc);
protected:
	ReadWriteService(int _objpermutbts = 6, int _mutrowsbts = 8, int _mutcolsbts = 8);
	virtual int insert(Object &_robj, ulong _srvid);
private:
	typedef std::stack<
		Condition*,
		std::vector<Condition*>
	> 								CondStackT; 
	typedef MutualStore<CondStackT>	ConditionStoreT;
	ConditionStoreT	cndstore;
};
}//namespace foundation
#endif
