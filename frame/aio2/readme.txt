Ideas of use:
-------------

1) Single socket object - echo server:


2) Multi socket object:

class RelayNode: public frame::aio::Object{
private:
	void onEvent(ReactorContext &_rctx);
	void onEvent(ReactorContext &_rctx, size_t _sz);
private:
	frame::aio::Stream<frame::aio::Socket>	s1;
	frame::aio::Stream<frame::aio::Socket>	s2;
	char									buf1[2][BUFCP];
	char									buf2[2][BUFCP];
};


void RelayNode::onEvent(ReactorContext &_rctx){
	if(_rctx.event().id == EventStartE){
		s1.asyncRecvSome(_rctx, buf1[0], BUFCP, Event());//fully asynchronous call
		s2.asyncRecvSome(_rctx, buf2[0], BUFCP, Event());//fully asynchronous call
	}else if(_rctx.event().id == EventSendE){
		if(!_rctx.error()){
			sock.asyncRecvSome(_rctx, buf, BUFCP, Event());//fully asynchronous call
		}else{
			this->stop(_rctx);
		}
	}else if(_rctx.event().id == EventStopE){
		this->stop(_rctx);
	}
}

void RelayNode::onEvent(ReactorContext &_rctx, size_t _sz){
}
