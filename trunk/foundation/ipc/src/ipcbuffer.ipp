/* Inline implementation file ipcbuffer.ipp
	
	Copyright 2012 Valentin Palade 
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

#ifdef NINLINES
#define inline
#endif

inline Buffer::Buffer(
	char *_pb,
	uint16 _bc,
	uint16 _dl
):bc(_bc), dl(_dl), pb(_pb){
}

inline Buffer::Buffer(const Buffer& _rbuf):bc(_rbuf.bc), dl(_rbuf.dl), pb(_rbuf.release()){
}

inline Buffer& Buffer::operator=(const Buffer& _rbuf){
	if(this != &_rbuf){
		bc = _rbuf.bc;
		dl = _rbuf.dl;
		pb = _rbuf.release();
	}
	return *this;
}

inline bool Buffer::empty()const{
	return pb == NULL;
}

inline void Buffer::reset(){
	this->type(Unknown);
	this->resend(0);
	this->flags(0);
	this->id(0);
}

inline void Buffer::reinit(char *_pb, uint16 _bc, uint16 _dl){
	clear();
	pb = _pb;
	bc = _bc;
	dl = _dl;
}

inline char *Buffer::buffer()const{
	return pb;
}

inline char *Buffer::data()const{
	return pb + headerSize();
}

inline uint32 Buffer::bufferSize()const{
	return dl + headerSize();
}

inline void Buffer::bufferSize(uint32 _sz){
	cassert(_sz >= headerSize());
	dl = ((int16)_sz) - headerSize();
}

inline uint32 Buffer::bufferCapacity()const{
	return bc;
}

inline uint32 Buffer::dataSize()const{
	return dl;
}

inline void Buffer::dataSize(uint32 _dl){
	dl = _dl;
}

inline uint32 Buffer::dataCapacity()const{
	return bc - headerSize();
}

inline char* Buffer::dataEnd()const{
	return data() + dataSize();
}

inline uint32 Buffer::dataFreeSize()const{
	return dataCapacity() - dataSize();
}

inline void Buffer::dataType(DataTypes _dt){
	uint8				dt = _dt;
	CRCValue<uint8>		crcval(dt);
	*reinterpret_cast<uint8*>(dataEnd()) = (uint8)crcval;
	++dl;
}

inline char *Buffer::release(uint32 &_cp)const{
	char* tmp = pb; pb = NULL; 
	_cp = bc; bc = 0; dl = 0;
	return tmp;
}

inline char *Buffer::release()const{
	uint32 cp;
	return release(cp);
}

inline uint8 Buffer::type()const{
	return reinterpret_cast<uint8*>(pb)[0];
}

inline void Buffer::type(uint8 _tp){
	reinterpret_cast<uint8*>(pb)[0] = _tp;
}


inline uint8 Buffer::resend()const{
	return reinterpret_cast<uint8*>(pb)[1];
} 

inline void Buffer::resend(uint8 _ri){
	reinterpret_cast<uint8*>(pb)[1] = _ri;
}

inline uint16 Buffer::flags()const{
	uint16	v;
	serialization::binary::load(pb + 2, v);
	return v;
}

inline void Buffer::flags(uint16 _flags){
	serialization::binary::store(pb + 2, _flags);
}

inline uint32 Buffer::id()const {
	uint32	v;
	serialization::binary::load(pb + 4, v);
	return v;
}

inline void Buffer::id(const uint32 _id){
	serialization::binary::store(pb + 4, _id);
}

inline uint32 Buffer::relay()const{
	uint32	v;
	serialization::binary::load(pb + BaseSize, v);
	return v;
}

inline void Buffer::relay(const uint32 _relayid){
	serialization::binary::store(pb + BaseSize, _relayid);
}

inline uint8 Buffer::updateCount()const{
	return updateSize() * reinterpret_cast<uint8*>(pb)[BaseSize + relaySize()];
}

inline void Buffer::updateInit(){
	this->flags(this->flags() | UpdateFlag);
	reinterpret_cast<uint8*>(pb)[BaseSize + relaySize()] = 0;
}

inline uint32 Buffer::update(const uint _pos)const{
	uint32 v;
	serialization::binary::load(pb + BaseSize + relaySize() + updateSize() + (_pos * sizeof(uint32)), v);
	return v;
}

inline void Buffer::updatePush(uint32 _upd){
	cassert(updateSize());
	uint8 *pu = reinterpret_cast<uint8*>(pb + BaseSize + relaySize());
	serialization::binary::store(pb + BaseSize + relaySize() + updateSize() + ((*pu) * sizeof(uint32)), _upd);
	++(*pu);
}

inline uint32 Buffer::relaySize()const{
	return (this->flags() & RelayFlag);
}

inline uint32 Buffer::updateSize()const{
	return (this->flags() & UpdateFlag);
}

inline uint32 Buffer::headerSize()const{
	return BaseSize + relaySize() + updateSize() + (updateCount() * sizeof(uint32));
}
#ifdef NINLINES
#undef inline
#endif
