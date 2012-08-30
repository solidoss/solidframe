/* Declarations file ipcbuffer.hpp
	
	Copyright 2012 Valentin Palade 
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

#ifndef FOUNDATION_IPC_BUFFER_HPP
#define FOUNDATION_IPC_BUFFER_HPP

#include "system/cassert.hpp"
#include "foundation/ipc/ipcservice.hpp"
#include "algorithm/serialization/binarybasic.hpp"

#ifndef UIPCBUFFERCAPACITY
#define UIPCBUFFERCAPACITY 4096
#endif

namespace foundation{
namespace ipc{

struct Controller;

struct BufferContext{
	BufferContext(uint _offset):offset(_offset), reqbufid(-1){}
	uint	offset;
	uint	reqbufid;
};

struct Buffer{
	enum{
		Capacity = UIPCBUFFERCAPACITY,
		BaseSize = 8,
		KeepAliveSize = 8,
		LastBufferId = 0xffffffff - 32,
		UpdateBufferId = 0xffffffff,//the id of a buffer containing only updates
	};
	enum Types{
		KeepAliveType = 1,
		DataType,
		ConnectingType,
		AcceptingType,
		Unknown
	};
	enum Flags{
		UpdateFlag = 1,//fixed position 
		CompressedFlag = 2,
		RelayFlag = 4,//fixed position
	};
	enum DataTypes{
		ContinuedSignal = 0,
		NewSignal,
		OldSignal
	};
	
	static char* allocate();
	static void deallocate(char *_buf);
	
	Buffer(
		char *_pb = NULL,
		uint16 _bc = 0,
		uint16 _dl = 0
	);
	Buffer(const Buffer& _rbuf);
	Buffer& operator=(const Buffer& _rbuf);
	~Buffer();
	
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
#include "ipcbuffer.ipp"
#endif

std::ostream& operator<<(std::ostream &_ros, const Buffer &_rb);

}//namespace ipc
}//namespace foundation

#endif
