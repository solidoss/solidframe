#include "clientserver/tcp/multiconnection.hpp"
#include "system/cassert.hpp"

namespace clientserver{
namespace tcp{
MultiConnection::ChannelStub::~ChannelStub(){
	delete pchannel;
	pchannel = NULL;
}

/*virtual*/ MultiConnection::~MultiConnection(){
}
/*virtual*/ int MultiConnection::accept(clientserver::Visitor &_roi){
}
bool MultiConnection::channelOk(unsigned _pos){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].pchannel->ok();
}
int MultiConnection::channelConnect(unsigned _pos, const AddrInfoIterator& _raddr){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	int rv = chnvec[_pos].pchannel->connect(_raddr);
	if(rv == NOK){
		if(!(chnvec[_pos].flags & ChannelStub::Request)){
			chnvec[_pos].flags |= ChannelStub::Request;
			reqvec.push_back(_pos);
		}
		chnvec[_pos].flags |= ChannelStub::IORequest;
	}
	return rv;
}
bool MultiConnection::channelIsSecure(unsigned _pos)const{
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].pchannel->isSecure();
}
int MultiConnection::channelSend(unsigned _pos, const char* _pb, uint32 _bl, uint32 _flags){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	int rv = chnvec[_pos].pchannel->send(_pb, _bl, _flags);
	if(rv == NOK){
		if(!(chnvec[_pos].flags & ChannelStub::Request)){
			chnvec[_pos].flags |= ChannelStub::Request;
			reqvec.push_back(_pos);
		}
		chnvec[_pos].flags |= ChannelStub::IORequest;
	}
	return rv;

}
int MultiConnection::channelRecv(unsigned _pos, char *_pb, uint32 _bl, uint32 _flags){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	int rv = chnvec[_pos].pchannel->recv(_pb, _bl, _flags);
	if(rv == NOK){
		if(!(chnvec[_pos].flags & ChannelStub::Request)){
			chnvec[_pos].flags |= ChannelStub::Request;
			reqvec.push_back(_pos);
		}
		chnvec[_pos].flags |= ChannelStub::IORequest;
	}
	return rv;

}
uint32 MultiConnection::channelRecvSize(unsigned _pos)const{
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].pchannel->recvSize();
}
const uint64& MultiConnection::channelSendCount(unsigned _pos)const{
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].pchannel->sendCount();
}
const uint64& MultiConnection::channelRecvCount(unsigned _pos)const{
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].pchannel->recvCount();
}
bool MultiConnection::channnelArePendingSends(unsigned _pos){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].pchannel->arePendingSends();
}
int MultiConnection::channnelArePendingRecvs(unsigned _pos){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].pchannel->arePendingRecvs();
}
int MultiConnection::channnelLocalAddress(unsigned _pos, SocketAddress &_rsa){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].pchannel->localAddress(_rsa);
}
int MultiConnection::channnelRemoteAddress(unsigned _pos, SocketAddress &_rsa){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].pchannel->remoteAddress(_rsa);
}
void MultiConnection::channelErase(unsigned _pos){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	
}
unsigned MultiConnection::channelAdd(Channel *_pch){
	unsigned pos = 0;
	if(chnstk.size()){
		pos = chnstk.top(); chnstk.pop();
		chnvec[pos].reset();
		chnvec[pos].pchannel = _pch;
	}else{
		chnvec.push_back(ChannelStub(_pch));
		pos = chnvec.size() - 1;
	}
	channelRegisterRequest(pos);
}

void MultiConnection::channelTimeout(
	unsigned _pos,
	const TimeSpec &_crttime,
	ulong _addsec,
	ulong _addnsec
){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	chnvec[_pos].timeout = _crttime;
	chnvec[_pos].timeout.add(_addsec, _addnsec);
	if(chnvec[_pos].toutpos < 0){
		chnvec[_pos].toutpos = toutvec.size();
		toutvec.push_back(_pos);
	}
	if(nextchntout > chnvec[_pos].timeout){
		nextchntout = chnvec[_pos].timeout;
	}
}
uint32 MultiConnection::channelEvents(unsigned _pos){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].flags;
}
void MultiConnection::channelErase(unsigned _pos){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	if(rv == NOK){
		if(!(chnvec[_pos].flags & ChannelStub::Request)){
			chnvec[_pos].flags |= ChannelStub::Request;
			reqvec.push_back(_pos);
		}
		chnvec[_pos].flags |= ChannelStub::EraseRequest;
	}
}
void MultiConnection::channelRegisterRequest(unsigned _pos){
	if(rv == NOK){
		if(!(chnvec[_pos].flags & ChannelStub::Request)){
			chnvec[_pos].flags |= ChannelStub::Request;
			reqvec.push_back(_pos);
		}
		chnvec[_pos].flags |= ChannelStub::RegisterRequest;
	}
}
void MultiConnection::channelUnregisterRequest(unsigned _pos){
	if(rv == NOK){
		if(!(chnvec[_pos].flags & ChannelStub::Request)){
			chnvec[_pos].flags |= ChannelStub::Request;
			reqvec.push_back(_pos);
		}
		chnvec[_pos].flags |= ChannelStub::UnregisterRequest;
	}
}

const UIntVectorTp & MultiConnection::signelledChannelsVector()const{
	return donevec;
}


void MultiConnection::clearRequestVector(){
	for(UIntVectorTp::const_iterator it(reqvec.begin()); it != reqvec.end(); ++it){
		chnvec[*it].flags = 0;
		chnvec[*it].chnevents = 0;
	}
	reqvec.clear();
}
void MultiConnection::clearDoneVector(){
	for(UIntVectorTp::const_iterator it(donevec.begin()); it != donevec.end(); ++it){
		chnvec[*it].flags = 0;
	}
	donevec.clear();
}

}//namespace tcp
}//namespace clientserver
