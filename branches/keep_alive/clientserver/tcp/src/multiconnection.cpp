#include "clientserver/tcp/multiconnection.hpp"
#include "system/cassert.hpp"

namespace clientserver{
namespace tcp{
MultiConnection::MultiConnection(Channel *_pch):nextchntout(0xffffffff,0xffffffff){
	if(_pch){
		chnvec.push_back(ChannelStub(_pch));
	}
}
MultiConnection::ChannelStub::~ChannelStub(){
}

/*virtual*/ MultiConnection::~MultiConnection(){
	for(ChannelVectorTp::iterator it(chnvec.begin()); it != chnvec.end(); ++it){
		delete it->pchannel;
	}
}
/*virtual*/ int MultiConnection::accept(clientserver::Visitor &_roi){
	return BAD;
}
bool MultiConnection::channelOk(unsigned _pos)const{
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
bool MultiConnection::channnelArePendingSends(unsigned _pos)const{
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].pchannel->arePendingSends();
}
bool MultiConnection::channnelArePendingRecvs(unsigned _pos)const{
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].pchannel->arePendingRecvs();
}
int MultiConnection::channnelLocalAddress(unsigned _pos, SocketAddress &_rsa)const{
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].pchannel->localAddress(_rsa);
}
int MultiConnection::channnelRemoteAddress(unsigned _pos, SocketAddress &_rsa)const{
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].pchannel->remoteAddress(_rsa);
}
// void MultiConnection::channelErase(unsigned _pos){
// 	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
// 	
// }
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
	return pos;
}

void MultiConnection::channelTimeout(
	unsigned _pos,
	const TimeSpec &_crttime,
	ulong _addsec,
	ulong _addnsec
){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	chnvec[_pos].timepos = _crttime;
	chnvec[_pos].timepos.add(_addsec, _addnsec);
	if(chnvec[_pos].toutpos < 0){
		chnvec[_pos].toutpos = toutvec.size();
		toutvec.push_back(_pos);
	}
	if(nextchntout > chnvec[_pos].timepos){
		nextchntout = chnvec[_pos].timepos;
	}
}
uint32 MultiConnection::channelEvents(unsigned _pos)const{
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].chnevents;
}
void MultiConnection::channelRegisterRequest(unsigned _pos){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	if(!(chnvec[_pos].flags & ChannelStub::Request)){
		chnvec[_pos].flags |= ChannelStub::Request;
		reqvec.push_back(_pos);
	}
	chnvec[_pos].flags |= ChannelStub::RegisterRequest;
}
void MultiConnection::channelUnregisterRequest(unsigned _pos){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	if(!(chnvec[_pos].flags & ChannelStub::Request)){
		chnvec[_pos].flags |= ChannelStub::Request;
		reqvec.push_back(_pos);
	}
	chnvec[_pos].flags |= ChannelStub::UnregisterRequest;
}
unsigned MultiConnection::channelCount()const{
	return chnvec.size();
}
int MultiConnection::channelState(unsigned _pos)const{
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	return chnvec[_pos].state;
}
void MultiConnection::channelState(unsigned _pos, int _st){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	chnvec[_pos].state = _st;
}

const MultiConnection::UIntVectorTp & MultiConnection::signelledChannelsVector()const{
	return resvec;
}

unsigned MultiConnection::channelRequestCount()const{
	return reqvec.size();
}
void MultiConnection::clearRequestVector(){
	idbgx(Dbg::tcp, "");
	for(UIntVectorTp::const_iterator it(reqvec.begin()); it != reqvec.end(); ++it){
		chnvec[*it].flags &= ~ChannelStub::AllRequests;
		//chnvec[*it].chnevents = 0;
	}
	reqvec.clear();
}
void MultiConnection::clearResponseVector(){
	idbgx(Dbg::tcp, "");
	for(UIntVectorTp::const_iterator it(resvec.begin()); it != resvec.end(); ++it){
		chnvec[*it].flags &= ~ChannelStub::AllResponses;
		chnvec[*it].chnevents = 0;
		//chnvec[*it].selevents = 0;
	}
	resvec.clear();
}

Channel* MultiConnection::channel(unsigned _pos){
	return chnvec[_pos].pchannel;
}

void MultiConnection::addTimeoutChannels(const TimeSpec &_crttime){
	nextchntout.set(0xffffffff, 0xffffffff);
	for(ChannelVectorTp::iterator it(chnvec.begin()); it != chnvec.end(); ++it){
		if(_crttime >= it->timepos){
			cassert(it->pchannel);
			cassert(it->toutpos >= 0);
			toutvec[it->toutpos] = toutvec.back();
			toutvec.pop_back();
			it->toutpos = -1;
			if(!(it->flags & ChannelStub::Response)){
				//add the channel to done vec
				resvec.push_back(it - chnvec.begin());
				it->flags |= ChannelStub::Response;
				it->chnevents |= TIMEOUT;//set the event to timeout
			}
		}else{
			if(it->timepos < nextchntout){
				nextchntout = it->timepos;
			}
		}
	}
}
void MultiConnection::addDoneChannelFirst(unsigned _pos, uint32 _evs){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	chnvec[_pos].chnevents |= _evs;
	//try erase it from toutvec:
	if(chnvec[_pos].toutpos >= 0){
		toutvec[chnvec[_pos].toutpos] = toutvec.back();
		toutvec.pop_back();
		chnvec[_pos].toutpos = -1;
	}
	//try add it to res vector:
	if(!(chnvec[_pos].flags & ChannelStub::Response)){
		resvec.push_back(_pos);
		chnvec[_pos].flags = ChannelStub::Response;
	}
}

void MultiConnection::addDoneChannelNext(unsigned _pos, uint32 _evs){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	idbgx(Dbg::tcp, "pos "<<_pos<<" evs = "<<_evs);
	chnvec[_pos].chnevents |= _evs;
	//try erase it from toutvec:
	if(chnvec[_pos].toutpos >= 0){
		toutvec[chnvec[_pos].toutpos] = toutvec.back();
		toutvec.pop_back();
		chnvec[_pos].toutpos = -1;
	}
	//try add it to res vector:
	if(!(chnvec[_pos].flags & ChannelStub::Response)){
		resvec.push_back(_pos);
		chnvec[_pos].flags |= ChannelStub::Response;
	}
}

void MultiConnection::channelErase(unsigned _pos){
	cassert(_pos < chnvec.size() && chnvec[_pos].pchannel);
	delete chnvec[_pos].pchannel;
	chnvec[_pos].reset();
	chnstk.push(_pos);
}

}//namespace tcp
}//namespace clientserver
