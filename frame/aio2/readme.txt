Ideas of use:
-------------

1) Single socket object - echo server:

class Connection: public frame::aio::Object<>{
public:
	Connection(SocketDevice &_rsd): s(_rsd, *this){
		cassert(s.id() == 0);
	}
	//v0
	void execute(Context &_rctx){
		error_code		err;
		size_t			rcvsz;
		
		switch(_rctx.event()){
			case DoneRecvEvent:
				s.doneRecv(recvsz, err);
				if(s.send(buf, recvs)){
					if(!err){
					}else{
						_rctx.reschedule(EnterEvent);
					}
				}//no break - we go forward
			case EnterEvent:
				if(s.recv(buf, 1024, recvsz, err, DoneRecvEvent)){
					if(!err){
						if(s.send(buf, recvs)){
							if(!err){
								_rctx.reschedule(EnterEvent);
								break;
							}
						}else{
							break;
						}
					}
				}else{
					break;
				}
			case KillEvent:
				_rctx.close();
				break;
		}
	}
	
	//v1
	void execute(Context &_rctx){
		if(_rctx.event() == 0){
			_rctx.close();
			return;
		}
		error_code		err;
		size_t			rcvsz;
		
		if(!s.hasPendingRecv() && !s.hasPendingSend()){
			s.recv(buf, 1024);
		}
		
		if(s.doneRecv(rcvsz, err)){
			if(s.sendAll(buf, rcvsz, err)){
				if(!err){
					_rctx.reschedule();
				}else{
					_rctx.close();
				}
			}else{
				_rctx.close();
			}
		}
	}
	
	//v0
	void execute(Context &_rctx){
		error_code		err;
		size_t			rcvsz;
		
		switch(_rctx.event()){
			case DoneRecvEvent:
				s.doneRecv(recvsz, err);
				if(s.send(buf, recvs)){
					if(!err){
					}else{
						_rctx.reschedule(EnterEvent);
					}
				}//no break - we go forward
			case EnterEvent:
				if(s.recv(buf, 1024, recvsz, err, DoneRecvEvent)){
					if(!err){
						if(s.send(buf, recvs)){
							if(!err){
								_rctx.reschedule(EnterEvent);
								break;
							}
						}else{
							break;
						}
					}
				}else{
					break;
				}
			case KillEvent:
				_rctx.close();
				break;
		}
	}

	
	//v3
	void execute(Context &_rctx){
		if(_rctx.event() == KillEvent){
			_rctx.close();
			return;
		}
		error_code		err;
		size_t			rcvsz;
		size_t			cnt = 16;
		
		if(s.hasPendingSend() || s.hasPendingRecv()){
			//keep waiting
			return;
		}
		
		do{
			if(s.recv(buf, 1024, rcvsz, err)){
				if(!err){
					if(s.sendAll(buf, rcvsz, err)){
						if(!err){
						}else{
							_rctx.close();
							return;
						}
					}else{
						break;
					}
				}else{
					_rctx.close();
					return;
				}
			}else{
				break;
			}
		}while(--cnt);
		
		_rctx.reschedule();
	}
	
private:
	frame::aio::StreamSocket	s;
	char						buf[1024];
};

class ListenObject: public frame::aio::Object<>{
public:
	ListenObject(SocketDevice &_rsd): s(_rsd, *this){
		cassert(s.id() == 0);
	}
	void execute(Context &_rctx){
		
		SocketDevice	sd;
		error_code		err;
		size_t			cnt = 16;
		
		if(s.doneAccept(sd, err)){
			Connection *pcon = new Connection(sd);
			//schedule pcon
			//...
		}
		
		do{
			if(s.accept(sd, err)){
				if(!err){
					Connection *pcon = new Connection(sd);
					//schedule pcon
					//...
				}else{
					t.waitFor(10);
				}
			}else break;
		}while(--cnt);
		
		if(!cnt){
			s.reschedule();
		}
	}
private:
	frame::aio::ListenSocket	s;
	frame::Timer				t;
};

2) Multi socket object:
class MyAioObject: public frame::aio::Object<>{
public:
	MyAioObject(SocketDevice &_rsd1, SocketDevice &_rsd2): ls(_rsd1, *this), ms(_rsd2, *this){
		cassert(ls.id() == 0);
		cassert(ms.id() == 0);
	}
	~MyAioObject(){
	}
	void execute(Context &_rctx){
		bool			must_reschedule;
		error_code		err;
		if(signaledSocketId() == 0){
			//use ls
			SocketDevice	sd;
			switch(ls.accept(sd, err)){
				case AsyncSuccess:{
					SocketStub *pss = new SocketStub(sd);
					size_t		pos = insertSocketStub(pss);
					registerSocket(pss->s, pos + 2);
					pss->s.reschedule();
					ls.reschedule();
				}
				break;
				case AsyncWait:
				break;
				case AsyncError:
					idbg("Error listen: "<<err);
					//todo - must retry later
				break;
			}
		}else if(signaledSocketId() == 1){
			
		}else if(signaledSocketId() != -1){
			SocketStub	&rss = svec[signaledSocketId() - 2];
			size_t		sz;
			if(rss.s.rescheduled()){
				switch(rss.s.recv(rss.rbuf, 1024, sz, err)){
					case AsyncSuccess:
						msndq.push(Stub(signaledSocketId() - 2, sz));
						rss.s.reschedule();
					break;
					case AsyncWait:
					case AsyncError:
						unregisterSocket(rss.s);
						eraseStub(signaledSocketId() - 2);
					break;
				}
			}
			if(rss.s.doneRecv(rcvsz, err)){
				msndq.push(Stub(signaledSocketId() - 2, sz));
			}
			if(rss.s.doneSend(sz, err)){
				
			}
		}else{
			//object level events
		}
	}
private:
	struct SocketStub{
		PlainSocket		s;
		char			rbuf[1024];
	};
	frame::aio::ListenSocket	ls;
	char 						mrbuf[1024];
	SecureSocket				ms;
	vector<PlainSocket*>		svec;
};