/* Implementation file channeldata.cpp
	
	Copyright 2007, 2008 Valentin Palade 
	vipalade@gmail.com

	This file is part of SolidGround framework.

	SolidGround is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Foobar is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with SolidGround.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "channeldata.h"
#include "system/specific.h"
#include "system/cassert.h"

namespace clientserver{
namespace tcp{

ChannelData* ChannelData::pop(){
	return Specific::uncache<ChannelData>();
}
void ChannelData::push(ChannelData *&_rpcd){
	while(_rpcd->psdnfirst){
		_rpcd->popSendData();
	}
	while(_rpcd->pssnfirst){
		_rpcd->popSendStream();
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

void ChannelData::pushSend(const IStreamIterator &_rsi, uint64 _sz){
	IStreamNode *psn = Specific::uncache<IStreamNode>();
	psn->sit = _rsi;
	psn->sz = _sz;
	psn->pnext = NULL;
	if(pssnlast){
		pssnlast->pnext = psn;
		pssnlast = psn;
	}else{
		pssnfirst = pssnlast = psn;
	}
}

void ChannelData::popSendData(){
	cassert(psdnfirst);
	DataNode	*psdn = psdnfirst->pnext;
	Specific::cache(psdnfirst);
	psdnfirst = psdn;
	if(!psdnfirst) psdnlast = NULL;
}

void ChannelData::popSendStream(){
	cassert(pssnfirst);
	IStreamNode	*pssn = pssnfirst->pnext;
	Specific::cache(pssnfirst);
	pssnfirst = pssn;
	if(!pssnfirst) pssnlast = NULL;
}

}//namespace tcp
}//namespace clientserver

