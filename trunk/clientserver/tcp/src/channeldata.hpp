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
#include "utility/istream.hpp"
#include "utility/ostream.hpp"

namespace clientserver{
namespace tcp{

struct DataNode{
	DataNode(
		char *_pb = NULL, 
		uint32 _bl = 0, 
		uint32 _flags = 0
	):bl(_bl), flags(_flags),pnext(NULL){
		b.pb = _pb;
	}
	DataNode(
		const char *_pb,
		uint32 _bl = 0,
		uint32 _flags = 0
	):bl(_bl), flags(_flags),pnext(NULL){
		b.pcb = _pb;
	}
	void reinit(char *_pb, uint32 _bl, uint32 _flags){
		b.pb = _pb; bl = _bl; flags = _flags;
	}
	void reinit(const char *_pb, uint32 _bl, uint32 _flags){
		b.pcb = _pb; bl = _bl; flags = _flags;
	}
	union{
		char		*pb;
		const char	*pcb;
	} b;
	uint32		bl;
	uint32		flags;
	DataNode	*pnext;
	static unsigned specificCount(){return 0xfffffff;}
	void specificRelease(){
// 		pnext = NULL;
// 		flags= 0;
	}
};

struct OStreamNode{
	OStreamNode():sz(0){}
	OStreamIterator		sit;
	uint64				sz;
};

struct IStreamNode{
	IStreamNode():sz(0),pnext(NULL){}
	IStreamIterator		sit;
	uint64				sz;
	IStreamNode			*pnext;
	static unsigned specificCount(){return 0xfffffff;}
	void specificRelease(){
// 		pnext = NULL;
// 		sz = 0;
	}
};

struct ChannelData{
	ChannelData():
		rcvsz(0), rcvoffset(0),
		psdnfirst(NULL), psdnlast(NULL), 
		pssnfirst(NULL), pssnlast(NULL), wcnt(0){}
	void clear(){
		rcvsz = 0; psdnfirst = NULL; psdnlast = NULL;
		pssnfirst = NULL; pssnlast = NULL; wcnt = 0;
	}
	static ChannelData* pop();
	static void push(ChannelData *&_rpcd);
	static unsigned specificCount(){return 0xfffffff;}
	void specificRelease(){
// 		pnext = NULL;
// 		sz = 0;
	}
	long arePendingSends(){return (long)psdnfirst;}
	void pushSend(const char *, uint32 _sz, uint32 _flags);
	void pushSend(const IStreamIterator &_rsi, uint64 _sz);
	void popSendData();
	void popSendStream();
	void setRecv(char *, uint32 _sz, uint32 _flags);
	void setRecv(const OStreamIterator &_rsi, uint64 _sz);
	uint32 				rcvsz;
	uint32				rcvoffset;
	DataNode			rdn;//ptr receive channel data
	OStreamNode			rsn;//ptr receive stream data
	DataNode			*psdnfirst,*psdnlast;//ptr send channel data 
	IStreamNode			*pssnfirst,*pssnlast;//ptr send stream data
	uint64				wcnt;
};

inline void ChannelData::setRecv(char *_pb, uint32 _sz, uint32 _flags){
	rdn.b.pb = _pb; rdn.bl = _sz; rdn.flags = _flags;
}
inline void ChannelData::setRecv(const OStreamIterator &_rsi, uint64 _sz){
	rsn.sit = _rsi; rsn.sz = _sz;
}

}
}
#endif

