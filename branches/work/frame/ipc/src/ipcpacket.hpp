// frame/ipc/src/ipcpacket.hpp
//
// Copyright (c) 2012,2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_PACKET_HPP
#define SOLID_FRAME_IPC_PACKET_HPP

#include "system/cassert.hpp"
#include "frame/ipc/ipcservice.hpp"
#include "serialization/binarybasic.hpp"

#ifndef UIPCPACKETCAPACITY
#define UIPCPACKETCAPACITY 4096
#endif

namespace solid{
namespace frame{
namespace ipc{

struct Controller;

struct PacketContext{
	PacketContext(uint _offset):offset(_offset), reqbufid(-1){}
	uint	offset;
	uint	reqbufid;
};

struct Packet{
	enum{
		Capacity = UIPCPACKETCAPACITY,
		BaseSize = 8,
		MinRelayReadSize = BaseSize + 4 + 2, // BaseSize + relayid + relay_data_size
		KeepAliveSize = 16,
		LastPacketId = 0xffffffff - 32,
		UpdatePacketId = 0xffffffff,//the id of a buffer containing only updates
	};
	enum Types{
		KeepAliveType = 1,
		DataType,
		ConnectType,
		AcceptType,
		ErrorType,
		Unknown
	};
	enum Flags{
		UpdateFlag = 1,//fixed position 
		CompressedFlag = 2,
		RelayFlag = 4,//fixed position
		DebugFlag = 8,
	};
	enum DataTypes{
		ContinuedMessage = 1,
		NewMessage,
		OldMessage
	};
	
	static char* allocate();
	static void deallocate(char *_pb);
	
	Packet(
		char *_pb = NULL,
		uint16 _bc = 0,
		uint16 _dl = 0
	);
	Packet(const Packet& _rpkt);
	Packet& operator=(const Packet& _rpkt);
	~Packet();
	
	bool empty()const;
	
	void reset();
	
	void clear();
	
	void optimize(uint16 _cp = 0);
	
	bool compress(Controller &_rctrl);
	bool decompress(Controller &_rctrl);
	
	bool check()const;
	
	void reinit(char *_pb = NULL, uint16 _bc = 0, uint16 _dl = 0);
	
	char *buffer()const;
	char *data()const ;
	
	char *release(uint32 &_cp)const;
	char *release()const;
	
	uint32 bufferSize()const;
	
	void bufferSize(const uint32 _sz);
	
	uint32 bufferCapacity()const;
	
	uint32 dataSize()const;
	void dataSize(const uint32 _dl);
	uint32 dataCapacity()const;
	
	char* dataEnd()const;
	uint32 dataFreeSize()const;
	
	void dataType(DataTypes _dt);
	
	uint8 version()const;
	void version(uint8 _v);
	
	uint8 resend()const;
	void resend(uint8 _ri);
	
	uint8 type()const;
	void type(uint8 _tp);
	
	uint16 flags()const;
	void flags(uint16 _flags);
	
	uint32 id()const;
	void id(const uint32 _id);
	
	uint32 relay()const;
	void relay(const uint32 _id);
	
	uint16 relayPacketSize()const;
	void relayPacketSizeStore();
	
	bool isRelay()const;
	
	uint8 updateCount()const;
	void updateInit();
	
	uint32 update(const uint _pos)const;
	void updatePush(const uint32 _upd);
	uint32 headerSize()const;
private:
	uint32 relaySize()const;
	uint32 updateSize()const;
//data
	mutable uint16	bc;//buffer capacity
	mutable int16	dl;//data length
	mutable char	*pb;
};

#ifndef NINLINES
#include "ipcpacket.ipp"
#endif

std::ostream& operator<<(std::ostream &_ros, const Packet &_rp);

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
