Ideas of use:
-------------

1) Single socket object - echo server:

class Connection: public frame::aio::Object<>{
public:
	Connection(SocketDevice &_rsd): s(_rsd, *this){
		cassert(s.id() == 0);
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

3) Relay node:

class RelayNode: public frame::aio::Object<2>{
public:
	void execute(Context &_rctx){
		
		error_code		err;
		size_t			rcvsz;
		size_t			cnt = 16;
		if(state == Relay){
			do{
				switch(s1.read(
			}
			
			
		}else if(state == RelayStart){
			switch(s1.read(b11, 1024, rcvsz, err)){
				case AsyncSuccess:
					s2.writeAll(b11, e
				case AsyncWait:
					break;
				case AsyncError:
					_rctx.close();
					break;
			}
			switch(s2.read(b21, 1024)){
				case AsyncSuccess:
				case AsyncWait:
					break;
				case AsyncError:
					_rctx.close();
					break;
			}
		}
		
	}
private:
	uchar						state;
	frame::aio::StreamSocket	s1;
	char						b11[1024];
	char						b12[1024];
	uchar						s1state;
	frame::aio::StreamSocket	s2;
	char						b21[1024];
	char						b22[1024];
	uchar						s2state;
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