/* Inline implementation file channeldata.ipp
	
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

#ifndef UINLINES
#define inline
#endif


inline DataNode::DataNode(
	char *_pb, 
	uint32 _bl, 
	uint32 _flags
):bl(_bl), flags(_flags),pnext(NULL){
	b.pb = _pb;
}

inline DataNode::DataNode(
	const char *_pb,
	uint32 _bl,
	uint32 _flags
):bl(_bl), flags(_flags),pnext(NULL){
	b.pcb = _pb;
}

inline void DataNode::reinit(char *_pb, uint32 _bl, uint32 _flags){
	b.pb = _pb; bl = _bl; flags = _flags;
}
inline void DataNode::reinit(const char *_pb, uint32 _bl, uint32 _flags){
	b.pcb = _pb; bl = _bl; flags = _flags;
}

inline /*static*/ unsigned DataNode::specificCount(){
return 0xfffffff;
}

inline void DataNode::specificRelease(){
}
	



inline ChannelData::ChannelData():
	rcvsz(0), /*rcvoffset(0),*/flags(0), /*wcnt(0),*/
	psdnfirst(NULL), psdnlast(NULL) 
{
}

inline void ChannelData::clear(){
	rcvsz = 0; /*rcvoffset = 0;*/ flags = 0;
// 	wcnt = 0;
	psdnfirst = NULL; psdnlast = NULL;
}

inline /*static*/ unsigned ChannelData::specificCount(){
	return 0xfffffff;
}

inline void ChannelData::specificRelease(){
}

inline long ChannelData::arePendingSends(){
	return (long)psdnfirst;
}

inline void ChannelData::setRecv(char *_pb, uint32 _sz, uint32 _flags){
	rdn.b.pb = _pb; rdn.bl = _sz; rdn.flags = _flags;
}

#ifndef UINLINES
#undef inline
#endif


