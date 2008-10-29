/* Declarations file multiconnection.hpp
	
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

#ifndef CS_TCP_MULTICONNECTION_HPP
#define CS_TCP_MULTICONNECTION_HPP

#include <vector>
#include "system/timespec.hpp"
#include "utility/stack.hpp"
#include "clientserver/core/object.hpp"
#include "clientserver/tcp/channel.hpp"

namespace clientserver{
namespace tcp{

class MultiConnectionSelector;
class Channel;

class MultiConnection: public Object{
public:
	enum {
		RAISE_ON_END = Channel::RAISE_ON_END, 
		TRY_ONLY = Channel::TRY_ONLY, 
		PREPARE_ONLY = Channel::PREPARE_ONLY
	};
	
	typedef std::vector<uint>			UIntVectorTp;
	
	virtual ~MultiConnection();
	virtual int execute(ulong _evs, TimeSpec &_rtout) = 0;
	virtual int accept(clientserver::Visitor &_roi);
//export channel interface:
	bool channelOk(unsigned _pos)const;
	int channelConnect(unsigned _pos, const AddrInfoIterator&);
	bool channelIsSecure(unsigned _pos)const;
	int channelSend(unsigned _pos, const char* _pb, uint32 _bl, uint32 _flags = 0);
	int channelRecv(unsigned _pos, char *_pb, uint32 _bl, uint32 _flags = 0);
	uint32 channelRecvSize(unsigned _pos)const;
	const uint64& channelSendCount(unsigned _pos)const;
	const uint64& channelRecvCount(unsigned _pos)const;
	bool channnelArePendingSends(unsigned _pos)const;
	bool channnelArePendingRecvs(unsigned _pos)const;
	int channnelLocalAddress(unsigned _pos, SocketAddress &_rsa)const;
	int channnelRemoteAddress(unsigned _pos, SocketAddress &_rsa)const;
	void channelTimeout(unsigned _pos, const TimeSpec &_crttime, ulong _addsec, ulong _addnsec);
	uint32 channelEvents(unsigned _pos)const;
	void channelErase(unsigned _pos);
	unsigned channelAdd(Channel *_pch);
	void channelRegisterRequest(unsigned _pos);
	void channelUnregisterRequest(unsigned _pos);
	unsigned channelCount()const;
	
	const UIntVectorTp & signelledChannelsVector()const;
protected:
	MultiConnection(Channel *_pch = NULL);
private:
	friend class MultiConnectionSelector;
	void clearRequestVector();
	void clearResponseVector();
	void addTimeoutChannels(const TimeSpec &_crttime);
	void addDoneChannel(unsigned _pos, uint32 _evs);
	Channel* channel(unsigned _pos);
private:
	struct ChannelStub{
		enum{
			//Requests from multiconnection to selector
			Request = 1,
			Response = 2,
			IORequest = 4,
			UnregisterRequest = 8,
			RegisterRequest = 16,
			EraseRequest = 32
		};
		ChannelStub(Channel *_pch = NULL):
			pchannel(_pch),
			timepos(0xffffffff, 0xffffffff),
			toutpos(-1),
			flags(0),chnevents(0),selevents(0){}
		~ChannelStub();
		void reset(){
			pchannel = NULL;
			timepos.set(0xffffffff, 0xffffffff);
			toutpos = -1;
			flags = 0;
			chnevents = 0;
			selevents = 0;
		}
		Channel		*pchannel;
		TimeSpec	timepos;
		int			toutpos;	//-1 or position in toutvec
		uint32		flags;		//Requests from connection to selector
		uint32		chnevents;	//the event from selector to connection:
								//INDONE, OUTDONE, TIMEOUT, ERRDONE
		uint32		selevents;	//used by selector - current io requests
	};
	typedef std::vector<ChannelStub>	ChannelVectorTp;
	typedef Stack<uint>					PositionStackTp;
	ChannelVectorTp		chnvec;//the channels
	PositionStackTp		chnstk;//keeps freepositions in chnvec
	UIntVectorTp		reqvec;//keeps channels with requests - used by channel selector
	UIntVectorTp		resvec;//keeps the signeld channels
	UIntVectorTp		toutvec;//keeps the channels expecting timeout
	TimeSpec			nextchntout;
};

}//namespace tcp
}//namespace clientserver

#endif
