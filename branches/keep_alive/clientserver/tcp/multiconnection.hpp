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
	virtual ~MultiConnection();
	virtual int execute(ulong _evs, TimeSpec &_rtout) = 0;
	virtual int accept(clientserver::Visitor &_roi);
//export channel interface:
	bool channelOk(unsigned _pos = 0);
	int channelConnect(const AddrInfoIterator&, unsigned _pos = 0);
	bool channelIsSecure(unsigned _pos = 0)const;
	int channelSend(const char* _pb, uint32 _bl, uint32 _flags = 0, unsigned _pos = 0);
	int channelRecv(char *_pb, uint32 _bl, uint32 _flags = 0, unsigned _pos = 0);
	uint32 channelRecvSize(unsigned _pos = 0)const;
	const uint64& channelSendCount(unsigned _pos = 0)const;
	const uint64& channelRecvCount(unsigned _pos = 0)const;
	bool channnelArePendingSends(unsigned _pos = 0);
	bool channnelArePendingRecvs(unsigned _pos = 0);
	int channnelLocalAddress(SocketAddress &_rsa, unsigned _pos = 0);
	int channnelRemoteAddress(SocketAddress &_rsa, unsigned _pos = 0);
	unsigned channelAdd();
protected:
	MultiConnection(Channel *_pch = NULL);
private:
	friend class MultiConnectionSelector;
	void clearRequestVector();
	void clearDoneVector();
private:
	struct ChannelStub{
		enum{
			Request = 1,
			Done = 2
		};
		ChannelStub(Channel *_pch):pchannel(_pch), flags(0){}
		~ChannelStub();
		Channel *pchannel;
		unsigned flags;
	};
	typedef std::vector<Channel*>	ChannelVectorTp;
	typedef std::vector<uint>		UIntVectorTp;
	ChannelVectorTp		channelv;
	UIntVectorTp		reqv;
	UIntVectorTp		donev;
};

}//namespace tcp
}//namespace clientserver
