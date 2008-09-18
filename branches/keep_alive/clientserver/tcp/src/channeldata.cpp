/* Implementation file channeldata.cpp
	
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

#include "channeldata.hpp"
#include "system/specific.hpp"
#include "system/cassert.hpp"

namespace clientserver{
namespace tcp{

#ifndef UINLINES
#include "channeldata.ipp"
#endif

ChannelData* ChannelData::pop(){
	return Specific::uncache<ChannelData>();
}
void ChannelData::push(ChannelData *&_rpcd){
	while(_rpcd->psdnfirst){
		_rpcd->popSendData();
	}
	Specific::cache(_rpcd);
	_rpcd = NULL;
}

void ChannelData::pushSend(const char *_pb, uint32 _sz, uint32 _flags){
	DataNode *pdn = Specific::uncache<DataNode>();
	pdn->b.pcb = _pb;
	pdn->bl = _sz;
	pdn->flags = _flags;
	pdn->pnext = NULL;
	if(psdnlast){
		psdnlast->pnext = pdn;
		psdnlast = pdn;
	}else{
		psdnfirst = psdnlast = pdn;
	}
}

void ChannelData::popSendData(){
	cassert(psdnfirst);
	DataNode	*psdn = psdnfirst->pnext;
	Specific::cache(psdnfirst);
	psdnfirst = psdn;
	if(!psdnfirst) psdnlast = NULL;
}

}//namespace tcp
}//namespace clientserver

