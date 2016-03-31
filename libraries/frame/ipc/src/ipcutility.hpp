// frame/ipc/src/ipcutility.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_IPC_SRC_IPC_UTILITY_HPP
#define SOLID_FRAME_IPC_SRC_IPC_UTILITY_HPP

#include "system/cassert.hpp"
#include "system/socketaddress.hpp"

#include "frame/ipc/ipcservice.hpp"

namespace solid{
namespace frame{
namespace ipc{

enum{
	MinimumFreePacketDataSize = 16,
	MaxPacketDataSize = 1024 * 64,
};


struct SocketAddressHash{
	size_t operator()(const SocketAddressInet* const &_rsa)const{
		return _rsa->hash();
	}
};

struct SocketAddressEqual{
	bool operator()(
		const SocketAddressInet* const &_rsa1,
		const SocketAddressInet* const &_rsa2
	)const{
		return *_rsa1 == *_rsa2;
	}
};

struct PacketHeader{
	enum {
        SizeOfE = 4,
    };
	
	enum Types{
		SwitchToNewMessageTypeE = 1,
		SwitchToOldMessageTypeE,
		ContinuedMessageTypeE,
		SwitchToOldCanceledMessageTypeE,
		ContinuedCanceledMessageTypeE,
		
		KeepAliveTypeE,
		
	};
	
	enum Flags{
		Size64KBFlagE = 1, // DO NOT CHANGE!!
		CompressedFlagE = 2,
	};
	
	PacketHeader(
		const uint8 _type = 0,
		const uint8 _flags = 0,
		const uint16 _size = 0
	){
		reset(_type, _flags, _size);
	}
	
	void reset(
		const uint8 _type = 0,
		const uint8 _flags = 0,
		const uint16 _size = 0
	){
		type(_type);
		flags(_flags);
		size(_size);
	}
	
	uint32 size()const{
		uint32 sz = (flags_ & Size64KBFlagE);
		return (sz << 16) | size_;
	}
	
	
	uint8 type()const{
		return type_;
	}
	
	uint8 flags()const{
		return flags_;
	}
	
	void type(uint8 _type){
		type_ = _type;
	}
	void flags(uint8 _flags){
		flags_ = _flags /*& (0xff - Size64KBFlagE)*/;
	}
	
	void size(uint32 _sz){
		size_ = _sz & 0xffff;
		flags_ |= ((_sz & (1 << 16)) >> 16);
	}
	
	bool isTypeKeepAlive()const{
		return type_ == KeepAliveTypeE;
	}
	
    bool isCompressed()const{
        return flags_ & CompressedFlagE;
    }
    bool isOk()const{
		bool rv = true;
		switch(type_){
			case SwitchToNewMessageTypeE:
			case SwitchToOldMessageTypeE:
			case ContinuedMessageTypeE:
			case SwitchToOldCanceledMessageTypeE:
			case ContinuedCanceledMessageTypeE:
			case KeepAliveTypeE:
				break;
			default:
				rv = false;
				break;
		}
		
		if(size() > MaxPacketDataSize){
			rv = false;
		}
		
		return rv;
	}
    
    template <class S>
    char* store(char * _pc)const{
		_pc = S::storeValue(_pc, type_);
		_pc = S::storeValue(_pc, flags_);
		_pc = S::storeValue(_pc, size_);
		return _pc;
	}
	
	template <class D>
    const char* load(const char *_pc){
		_pc = D::loadValue(_pc, type_);
		_pc = D::loadValue(_pc, flags_);
		_pc = D::loadValue(_pc, size_);
		return _pc;
	}
private:
	uint8	type_;
	uint8	flags_;
	uint16	size_;
};

struct MessageBundle{
	size_t						message_type_id;
	ulong						message_flags;
	
	MessagePointerT				message_ptr;
	MessageCompleteFunctionT	complete_fnc;
	
	MessageBundle():message_type_id(InvalidIndex()), message_flags(0){}
	
	MessageBundle(
		MessagePointerT &_rmsgptr,
		const size_t _msg_type_idx,
		ulong _flags,
		MessageCompleteFunctionT &_complete_fnc
	):	message_type_id(_msg_type_idx), message_flags(_flags), message_ptr(std::move(_rmsgptr)),
		complete_fnc(std::move(_complete_fnc)){}
	
	MessageBundle(
		MessageBundle && _rmsgbundle
	):	message_type_id(_rmsgbundle.message_type_id), message_flags(_rmsgbundle.message_flags),
		message_ptr(std::move(_rmsgbundle.message_ptr)), complete_fnc(std::move(_rmsgbundle.complete_fnc))
	{
		
	}
	
	MessageBundle& operator=(MessageBundle && _rmsgbundle){
		message_type_id = _rmsgbundle.message_type_id;
		message_flags = _rmsgbundle.message_flags;
		message_ptr = std::move(_rmsgbundle.message_ptr);
		complete_fnc = std::move(_rmsgbundle.complete_fnc);
		return *this;
	}
	
	void clear(){
		message_type_id = InvalidIndex();
		message_flags = 0;
		message_ptr.clear();
		FUNCTION_CLEAR(complete_fnc);
	}
};

}//namespace ipc
}//namespace frame
}//namespace solid

#endif
