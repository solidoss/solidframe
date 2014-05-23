Ideas of use:
-------------

1) Single socket object:
class Connection: public frame::aio::Object<>{
public:
	Connection(SocketDevice &_rsd): s(_rsd, *this){
		cassert(s.id() == 0);
	}
	void execute(Context &_rctx){
		
		error_code		err;
		size_t			rcvsz;
		
		if(s.doneRecv(rcvsz, err)){
		}else{
			switch(s.recv(buf, 1024, rcvsz, err)){
				case AsyncSuccess:
				case AsyncWait:
				case AsyncError:
				
			}
		}
	}
private:
	frame::aio::PlainSocket s;
	char					buf[1024];
};

class ListenObject: public frame::aio::Object<>{
public:
	ListenObject(SocketDevice &_rsd): s(_rsd, *this){
		cassert(s.id() == 0);
	}
	void execute(Context &_rctx){
		
		SocketDevice	sd;
		error_code		err;
		switch(s.accept(sd, err)){
			case AsyncSuccess:{
				Connection *pcon = new Connection(sd);
				//schedule pcon
				s.reschedule();
			}	break;
			case AsyncWait:
				break;
			case AsyncError:
				_rctx.waintFor(10);
				break;
		}
	}
private:
	frame::aio::ListenSocket s;
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