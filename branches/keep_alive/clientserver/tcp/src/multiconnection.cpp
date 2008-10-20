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
	cassert(_pos < channelv.size() && channelv[_pos].pchannel);
	return channelv[_pos].pchannel->ok();
}
int MultiConnection::channelConnect(const AddrInfoIterator& _raddr, unsigned _pos){
	cassert(_pos < channelv.size() && channelv[_pos].pchannel);
	int rv = channelv[_pos].pchannel->connect(_raddr);
	if(rv == NOK && !(channelv[_pos].flags & ChannelStub::Request)){
		channelv[_pos].flags |= ChannelStub::Request;
	}
	return rv;
}
bool MultiConnection::channelIsSecure(unsigned _pos)const{
	cassert(_pos < channelv.size() && channelv[_pos].pchannel);
	return channelv[_pos].pchannel->isSecure();
}
int MultiConnection::channelSend(const char* _pb, uint32 _bl, uint32 _flags, unsigned _pos){
	cassert(_pos < channelv.size() && channelv[_pos].pchannel);
	int rv = channelv[_pos].pchannel->send(_pb, _bl, _flags);
	if(rv == NOK && !(channelv[_pos].flags & ChannelStub::Request)){
		channelv[_pos].flags |= ChannelStub::Request;
	}
	return rv;

}
int MultiConnection::channelRecv(char *_pb, uint32 _bl, uint32 _flags, unsigned _pos){
	cassert(_pos < channelv.size() && channelv[_pos].pchannel);
	int rv = channelv[_pos].pchannel->recv(_pb, _bl, _flags);
	if(rv == NOK && !(channelv[_pos].flags & ChannelStub::Request)){
		channelv[_pos].flags |= ChannelStub::Request;
	}
	return rv;

}
uint32 MultiConnection::channelRecvSize(unsigned _pos)const{
	cassert(_pos < channelv.size() && channelv[_pos]);
	return channelv[_pos].pchannel->recvSize();
}
const uint64& MultiConnection::channelSendCount(unsigned _pos)const{
	cassert(_pos < channelv.size() && channelv[_pos].pchannel);
	return channelv[_pos].pchannel->sendCount();
}
const uint64& MultiConnection::channelRecvCount(unsigned _pos)const{
	cassert(_pos < channelv.size() && channelv[_pos].pchannel);
	return channelv[_pos].pchannel->recvCount();
}
bool MultiConnection::channnelArePendingSends(unsigned _pos){
	cassert(_pos < channelv.size() && channelv[_pos].pchannel);
	return channelv[_pos].pchannel->arePendingSends();
}
int MultiConnection::channnelArePendingRecvs(unsigned _pos){
	cassert(_pos < channelv.size() && channelv[_pos].pchannel);
	return channelv[_pos].pchannel->arePendingRecvs();
}
int MultiConnection::channnelLocalAddress(SocketAddress &_rsa, unsigned _pos){
	cassert(_pos < channelv.size() && channelv[_pos].pchannel);
	return channelv[_pos].pchannel->localAddress(_rsa);
}
int MultiConnection::channnelRemoteAddress(SocketAddress &_rsa, unsigned _pos){
	cassert(_pos < channelv.size() && channelv[_pos].pchannel);
	return channelv[_pos].pchannel->remoteAddress(_rsa);
}
unsigned MultiConnection::channelAdd(){
	//TODO:
}

void MultiConnection::clearRequestVector(){
	for(UIntVectorTp::const_iterator it(req.begin()); it != req.end(); ++it){
		channelv[*it].flags &= ~ChannelStub::Request;
	}
}
void MultiConnection::clearDoneVector(){
	for(UIntVectorTp::const_iterator it(req.begin()); it != req.end(); ++it){
		channelv[*it].flags &= ~ChannelStub::Done;
	}
}

}//namespace tcp
}//namespace clientserver
