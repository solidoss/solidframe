// frame/ipc/src/ipcpacket.ipp
//
// Copyright (c) 2012, 2013 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifdef NINLINES
#define inline
#endif

inline Packet::Packet(
	char *_pb,
	uint16 _bc,
	uint16 _dl
):bc(_bc), dl(_dl), pb(_pb){
}

inline Packet::Packet(const Packet& _rpkt):bc(_rpkt.bc), dl(_rpkt.dl), pb(_rpkt.release()){
}

inline Packet& Packet::operator=(const Packet& _rpkt){
	if(this != &_rpkt){
		bc = _rpkt.bc;
		dl = _rpkt.dl;
		pb = _rpkt.release();
	}
	return *this;
}

inline bool Packet::empty()const{
	return pb == NULL;
}

inline void Packet::reset(){
	this->type(Unknown);
	this->resend(0);
	this->flags(0);
	this->id(0);
	uint8 *pu = reinterpret_cast<uint8*>(pb + BaseSize + relaySize());
	*pu = 0;
}

inline void Packet::reinit(char *_pb, uint16 _bc, uint16 _dl){
	clear();
	pb = _pb;
	bc = _bc;
	dl = _dl;
}

inline char *Packet::buffer()const{
	return pb;
}

inline char *Packet::data()const{
	return pb + headerSize();
}

inline uint32 Packet::bufferSize()const{
	return dl + headerSize();
}

inline void Packet::bufferSize(uint32 _sz){
	cassert(_sz >= headerSize());
	dl = ((int16)_sz) - headerSize();
}

inline uint32 Packet::bufferCapacity()const{
	return bc;
}

inline uint32 Packet::dataSize()const{
	return dl;
}

inline void Packet::dataSize(uint32 _dl){
	dl = _dl;
}

inline uint32 Packet::dataCapacity()const{
	return bc - headerSize();
}

inline char* Packet::dataEnd()const{
	return data() + dataSize();
}

inline uint32 Packet::dataFreeSize()const{
	return dataCapacity() - dataSize();
}

inline void Packet::dataType(DataTypes _dt){
	uint8				dt = _dt;
	CRCValue<uint8>		crcval(dt);
	*reinterpret_cast<uint8*>(dataEnd()) = (uint8)crcval;
	++dl;
}

inline char *Packet::release(uint32 &_cp)const{
	char* tmp = pb; pb = NULL; 
	_cp = bc; bc = 0; dl = 0;
	return tmp;
}

inline char *Packet::release()const{
	uint32 cp;
	return release(cp);
}

inline uint8 Packet::type()const{
	return reinterpret_cast<uint8*>(pb)[0];
}

inline void Packet::type(uint8 _tp){
	reinterpret_cast<uint8*>(pb)[0] = _tp;
}


inline uint8 Packet::resend()const{
	return reinterpret_cast<uint8*>(pb)[1];
} 

inline void Packet::resend(uint8 _ri){
	reinterpret_cast<uint8*>(pb)[1] = _ri;
}

inline uint16 Packet::flags()const{
	uint16	v;
	serialization::binary::load(pb + 2, v);
	return v;
}

inline void Packet::flags(uint16 _flags){
	serialization::binary::store(pb + 2, _flags);
}

inline uint32 Packet::id()const {
	uint32	v;
	serialization::binary::load(pb + 4, v);
	return v;
}

inline void Packet::id(const uint32 _id){
	serialization::binary::store(pb + 4, _id);
}

inline uint32 Packet::relay()const{
	uint32	v;
	serialization::binary::load(pb + BaseSize, v);
	return v;
}

inline void Packet::relay(const uint32 _relayid){
	this->flags(this->flags() | RelayFlag);
	serialization::binary::store(pb + BaseSize, _relayid);
}

inline uint16 Packet::relayPacketSize()const{
	uint16	v;
	serialization::binary::load(pb + BaseSize + sizeof(uint32), v);
	return v;
}
inline void Packet::relayPacketSizeStore(){
	cassert(isRelay());
	const uint16 v = this->bufferSize();
	serialization::binary::store(pb + BaseSize + sizeof(uint32), v);
}

inline bool Packet::isRelay()const{
	return flags() & RelayFlag;
}

inline uint8 Packet::updateCount()const{
	return updateSize() * reinterpret_cast<uint8*>(pb)[BaseSize + relaySize()];
}

inline void Packet::updateInit(){
	this->flags(this->flags() | UpdateFlag);
	reinterpret_cast<uint8*>(pb)[BaseSize + relaySize()] = 0;
}

inline uint32 Packet::update(const uint _pos)const{
	uint32 v;
	serialization::binary::load(pb + BaseSize + relaySize() + updateSize() + (_pos * sizeof(uint32)), v);
	return v;
}

inline void Packet::updatePush(uint32 _upd){
	cassert(updateSize());
	uint8 *pu = reinterpret_cast<uint8*>(pb + BaseSize + relaySize());
	serialization::binary::store(pb + BaseSize + relaySize() + updateSize() + ((*pu) * sizeof(uint32)), _upd);
	++(*pu);
}

inline uint32 Packet::relaySize()const{
	const uint32 r = this->flags() & RelayFlag;
	return r + (r >> 1);// 6 bytes - 4 for relayid, 2 for buffer size 
}

inline uint32 Packet::updateSize()const{
	return (this->flags() & UpdateFlag);
}

inline uint32 Packet::headerSize()const{
	return BaseSize + relaySize() + updateSize() + (updateCount() * sizeof(uint32));
}
#ifdef NINLINES
#undef inline
#endif
