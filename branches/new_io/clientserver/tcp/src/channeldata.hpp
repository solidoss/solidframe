/* Declarations file channeldata.hpp
	
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

#ifndef CHANNEL_DATA_HPP
#define CHANNEL_DATA_HPP
#include <utility>
#include "system/common.hpp"

namespace clientserver{
namespace tcp{

struct DataNode{
	DataNode(char *_pb = NULL, uint32 _bl = 0, uint32 _flags = 0);
	DataNode(const char *_pb, uint32 _bl = 0, uint32 _flags = 0);
	void reinit(char *_pb, uint32 _bl, uint32 _flags);
	void reinit(const char *_pb, uint32 _bl, uint32 _flags);
	static unsigned specificCount();
	void specificRelease();
	union{
		char		*pb;
		const char	*pcb;
	} b;
	uint32		bl;
	uint32		flags;
	DataNode	*pnext;
};

struct ChannelData{
	ChannelData();
	void clear();
	static ChannelData* pop();
	static void push(ChannelData *&_rpcd);
	static unsigned specificCount();
	void specificRelease();
	long arePendingSends();
	void pushSend(const char *_pb, uint32 _sz, uint32 _flags);
	void popSendData();
	void setRecv(char *_pb, uint32 _sz, uint32 _flags);
	
	uint32 				rcvsz;
//	uint32				rcvoffset;
	uint32				flags;
	
	DataNode			rdn;//receive channel data
	DataNode			*psdnfirst,*psdnlast;//ptr send channel data 
};

#ifdef UINLINES
#include "channeldata.ipp"
#endif

}
}
#endif

