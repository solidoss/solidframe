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
				switch(s.send(buf, recvs)){
					case AsyncSuccess:
						break;
					case AsyncWait:
						return;
					case AsyncError:
						_rctx.reschedule(KillEvent);
						return;
				}//no break - we go forward
			case EnterEvent:
				switch(s.recv(buf, 1024, recvsz, err, DoneRecvEvent)){
					case AsyncSuccess:
						switch(s.send(buf, recvs)){
							case AsyncSuccess:
								_rctx.reschedule(EnterEvent);
								break;
							case AsyncWait:
								break;
							case AsyncError:
								_rctx.reschedule(KillEvent);
								break;
						}
						break;
					case AsyncWait:
						break;
					case AsyncError:
						_rctx.reschedule(KillEvent);
						break;
				}break;
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
			switch(s.recv(buf, 1024)){
				case AsyncSuccess:
					break;
				case AsyncWait:
					return;
				case AsyncError:
					_rctx.close();
					return;
			}
		}
		
		if(s.doneRecv(rcvsz, err)){
			switch(s.sendAll(buf, rcvsz)){
				case AsyncSuccess:
					s.reschedule();
					break;
				case AsyncWait:
					return;
				case AsyncError:
					_rctx.close();
					return;
			}
		}
	}
	
	//v2
	void execute(Context &_rctx){
		if(_rctx.eventClose()){
			_rctx.close();
			return;
		}
		error_code		err;
		size_t			rcvsz;
		size_t			cnt = 16;
		
		if(s.doneRecv(rcvsz, err)){
			switch(s.sendAll(buf, rcvsz)){
				case AsyncSuccess:
					break;
				case AsyncWait:
					return;
				case AsyncError:
					_rctx.close();
					return;
			}
		}
		
		if(s.hasPendingRecv() || s.hasPendingSend()){
			return;
		}
		
		do{
			switch(s.recv(buf, 1024, rcvsz, err)){
				case AsyncSuccess:
					switch(s.sendAll(buf, rcvsz)){
						case AsyncSuccess:
							break;
						case AsyncWait:
							return;
						case AsyncError:
							_rctx.close();
							return;
					}break;
				case AsyncWait:
					return;
				case AsyncError:
					_rctx.close();
					return;
			}
		}while(--cnt);
		
	}
	
	//v3
	void execute(Context &_rctx){
		if(_rctx.eventClose()){
			_rctx.close();
			return;
		}
		error_code		err;
		size_t			rcvsz;
		size_t			cnt = 16;
		
		if(s.hasPendingSend()){
			//keep waiting
			return;
		}
		do{
			switch(s.recv(buf, 1024, rcvsz, err)){
				case AsyncSuccess:
					switch(s.sendAll(buf, rcvsz)){
						case AsyncSuccess:
							break;
						case AsyncWait:
							return;
						case AsyncError:
							_rctx.close();
							return;
					}break;
				case AsyncWait:
					return;
				case AsyncError:
					_rctx.close();
					return;
			}
		}while(--cnt);
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
			switch(s.accept(sd, err)){
				case AsyncSuccess:{
					Connection *pcon = new Connection(sd);
					//schedule pcon
					//...
				}	break;
				case AsyncWait:
					return;
				case AsyncError:
					_rctx.waintFor(10);
					return;
			}
		}while(--cnt);
		if(!cnt){
			s.reschedule();
		}
	}
private:
	frame::aio::ListenSocket s;
};

3) Relay node - one buffer per connection:

class RelayNode: public frame::aio::Object<2>{
public:
	void execute(Context &_rctx){
		
		error_code		err;
		size_t			rcvsz;
		size_t			cnt = 16;
		switch(_rctx.event()){
			case DoneRecv_0_Event:
				sv[0].s.doneRecv(rcvsz, err);
				if(!err){
					
				}else{
					_rctx.reschedule(KillEvent);
					break;
				}break;
			case DoneRecv_1_Event:
				
			case StartRead_0_Event:
				if(sv[0].s.recv(sv[0].b, 1024, rcvsz, cnt, err, DoneRecv_0_Event)){
					if(!err){
						if(sv[1].s.sendAll(sv[0].b, rcvsz, cnt, err,StartRead_0_Event)){
							if(!err){
								_rctx.reschedule(StartRead_0_Event);
							}else{
								_rctx.reschedule(KillEvent);
							}
						}//else - wait
					}else{
						_rctx.reschedule(KillEvent);
						break;
					}
				}break;
				
			case StartRead_1_Event:
				switch(sv[1].s.recv(sv[1].b, 1024, rcvsz, cnt, err, DoneRecv_1_Event)){
					case AsyncSuccess:
						switch(sv[0].s.send(sv[1].b, rcvsz, cnt, StartRead_1_Event)){
							case AsyncSuccess:
								_rctx.reschedule(StartRead_1_Event);
								break;
							case AsyncWait:
							case AsyncError:
								_rctx.reschedule(KillEvent);
								break;
						}
					case AsyncWait:
						break;
					case AsyncError:
						
						break;
				}break;
			case EnterEvent:
				_rctx.reschedule(StartRead_0_Event);
				_rctx.reschedule(StartRead_1_Event);
				break;
			case KillEvent:
				_rctx.close();
				break;
			default:
				//ignore any unknown event
				break;
		}
	}
private:
	struct Stub{
		frame::aio::StreamSocket	s;
		char						b[1024];
	};
	Stub		sv[2];
};


3') Relay node - one 2 buffers per connection:

class RelayNode: public frame::aio::Object<2>{
public:
	void execute(Context &_rctx){
		
		error_code		err;
		size_t			rcvsz;
		size_t			cnt = 16;
		if(state == Relay){
			if(hasCurrentSocketId()){
				Stub	&rs = stubs[currentSocketId()];
				if(rs.s.doneRecv(rcvsz, err)){
					if(!err){
						Stub &rs2 = stubs[otherSocketId()];
						
					}else{
						_rctx.close();
						return;
					}
				}
			}
		}else if(state == RelayStart){
			switch(stubs[0].s.asyncRead(b11, 1024)){
				case AsyncSuccess:
					cassert(false);break;
				case AsyncWait:
					break;
				case AsyncError:
					_rctx.close();
					return;
			}
			switch(stubs[1].s.asyncRead(b21, 1024)){
				case AsyncSuccess:
					cassert(false);break;
				case AsyncWait:
					break;
				case AsyncError:
					_rctx.close();
					return;
			}
		}
		
	}
private:
	struct Stub{
		frame::aio::StreamSocket	s;
		char						b1[1024];
		char						b2[1024];
	};
	Stub		stubs[2];
};

3') Relay node - one 2 buffers per connection:

class RelayNode: public frame::aio::Object<2>{
public:
	void execute(Context &_rctx){
		
		error_code		err;
		size_t			rcvsz;
		size_t			cnt = 16;
		if(state == Relay){
			if(hasCurrentSocketId()){
				Stub	&rs1 = stubs[currentSocketId()];
				Stub	&rs2 = stubs[otherSocketId(currentSocketId())];
				if(rs1.s.doneRecv(rcvsz, err)){
					if(!err){
						switch(rs2.s.sendAll(rs1.b, rcvsz, err)){
							case AsyncSuccess:
								break;
							case AsyncWait;
								break;
							case AsyncError:
								_rctx.close();
								break;
						}
					}else{
						_rctx.close();
						return;
					}
				}
				if(rs1.s.doneSend(err)){
					if(!err){
						switch(rs2.s.asyncRead(rs2.b, 1024)){
							case AsyncSuccess:
								cassert(false);break;
							case AsyncWait:
								break;
							case AsyncError:
								_rctx.close();
								return;
						}
					}else{
						_rctx.close();
						return;
					}
				}
				while(cnt && !rs1.s.hasPendingRecv() && !rs2.s.hasPendingSend()){
					--cnt;
					switch(rs1.s.recvSome(rs1.b, 1024, rcvsz, err)){
						case AsyncSuccess:
							switch(rs2.s.sendAll(rs1.b, rcvsz, err)){
								case AsyncSuccess:
									break;
								case AsyncWait;
									break;
								case AsyncError:
									_rctx.close();
									break;
							}break;
						case AsyncWait:
							break;
						case AsyncError:
							_rctx.close();
							return;
					}
				}
				if(!rs1.s.hasPendingRecv() && !rs2.s.hasPendingSend()){
					rs1.s.reschedule();
				}
			}
		}else if(state == RelayStart){
			switch(stubs[0].s.asyncRead(stubs[0].b, 1024)){
				case AsyncSuccess:
					cassert(false);break;
				case AsyncWait:
					break;
				case AsyncError:
					_rctx.close();
					return;
			}
			switch(stubs[1].s.asyncRead(stubs[1].b, 1024)){
				case AsyncSuccess:
					cassert(false);break;
				case AsyncWait:
					break;
				case AsyncError:
					_rctx.close();
					return;
			}
		}
		
	}
private:
	struct Stub{
		frame::aio::StreamSocket	s;
		char						b[1024];
	};
	Stub		stubs[2];
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