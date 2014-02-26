#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "utility/queue.hpp"
#include "utility/stack.hpp"
#include <vector>
#include <iostream>

using namespace solid;
using namespace std;


struct BaseMessage{
	enum{
		FirstType = 0,
		SecondType,
		ThirdType,
		FourthType,
		FifthType,
		SixthType,
		SeventhType,
		MaxType,
	};
	const uint8 type;
	BaseMessage(const uint8 _type):type(_type){}
};

struct FirstMessage: BaseMessage{
	const uint16	v;
	FirstMessage(size_t _prdidx, size_t _idx):BaseMessage(FirstType), v(_prdidx ^ _idx){}
};

struct SecondMessage: BaseMessage{
	const uint32	v;
	SecondMessage(size_t _prdidx, size_t _idx):BaseMessage(SecondType), v(_prdidx ^ _idx){}
};

struct ThirdMessage: BaseMessage{
	const uint64	v;
	ThirdMessage(size_t _prdidx, size_t _idx):BaseMessage(ThirdType), v(_prdidx ^ _idx){}
};

struct FourthMessage: BaseMessage{
	const uint64	v1;
	const uint32	v2;
	FourthMessage(size_t _prdidx, size_t _idx):BaseMessage(FourthType), v1(_prdidx), v2(_idx){}
};

struct FifthMessage: BaseMessage{
	const uint64	v1;
	const uint64	v2;
	FifthMessage(size_t _prdidx, size_t _idx):BaseMessage(FifthType), v1(_prdidx), v2(_idx){}
};

struct SixthMessage: BaseMessage{
	const uint64	v1;
	const uint64	v2;
	const uint32	v3;
	SixthMessage(size_t _prdidx, size_t _idx):BaseMessage(SixthType), v1(_prdidx), v2(_idx), v3(_prdidx ^ _idx){}
};

struct SeventhMessage: BaseMessage{
	const uint64	v1;
	const uint64	v2;
	const uint64	v3;
	SeventhMessage(size_t _prdidx, size_t _idx):BaseMessage(SeventhType), v1(_prdidx), v2(_idx), v3(_prdidx ^ _idx){}
};

//#define USE_CACHE

class Consumer: private Thread{
public:
	Consumer(const uint64 _waitcnt):waitcnt(_waitcnt){}
	void start(){
		Thread::start(true);
	}
#ifndef USE_CACHE
	template <class T>
	void push(T _t){
		Locker<Mutex>	lock(mtx);
		msgq.push(new T(_t));
		cnd.signal();
	}
#else
	char * cachePop(const size_t _sz){
		if(_sz <= sizeof(uint64)){
			if(bufstks[0].size()){
				char *pb = bufstks[0].top();
				bufstks[0].pop();
				return pb;
			}else{
				return new char[sizeof(uint64)];
			}
		}else if(_sz <= 2*sizeof(uint64)){
			if(bufstks[1].size()){
				char *pb = bufstks[1].top();
				bufstks[1].pop();
				return pb;
			}else{
				return new char[sizeof(uint64)*2];
			}
		}else if(_sz <= 3*sizeof(uint64)){
			if(bufstks[2].size()){
				char *pb = bufstks[2].top();
				bufstks[2].pop();
				return pb;
			}else{
				return new char[sizeof(uint64)*3];
			}
		}else if(_sz <= 4*sizeof(uint64)){
            if(bufstks[3].size()){
                char *pb = bufstks[3].top();
                bufstks[3].pop();
                return pb;
            }else{
                return new char[sizeof(uint64)*4];
            }
        }else{
			return new char[_sz];
		}
	}
	void cacheSize2Index(solid::uint &_ridx, const size_t _sz)const{
		if(_sz <= sizeof(uint64)){
			_ridx = 0;
		}else if(_sz <= 2*sizeof(uint64)){
			_ridx = 1;
		}else if(_sz <= 3*sizeof(uint64)){
			_ridx = 2;
		}else if(_sz <= 4*sizeof(uint64)){
            _ridx = 3;
        }else{
			_ridx = 1000;
		}
	}
	template <class T>
	void push(T _t){
		Locker<Mutex>	lock(mtx);
		msgq.push(new(cachePop(sizeof(T))) T(_t));
		cnd.signal();
	}
#endif
private:
	void run();
private:
	typedef Queue<BaseMessage*, 10>		MessageQueueT;
	typedef Stack<char*, 10>			BufferStackT;
	uint64			waitcnt;
	uint64			sum;
	Mutex			mtx;
	Condition		cnd;
	MessageQueueT	msgq;
	BufferStackT	bufstks[4];
};

class Producer: private Thread{
	Consumer		&rcns;
	const size_t	idx;
	const size_t	cnt;
public:
	Producer(Consumer &_rcns, size_t _idx, size_t _cnt):rcns(_rcns), idx(_idx), cnt(_cnt){}
	void start(){
		Thread::start(true);
	}
private:
	void run();
};

int main(int argc, char * argv[]){
	
	if(argc != 3){
		cout<<"Usage:\n\t$ example_allocatetest 3 1000\n\t\tWhere 3 is the number of producers and 1000 is the number of iterations\n";
		return 0;
	}
	
	uint64			prdcnt = atoi(argv[1]);
	uint64			rptcnt = atoi(argv[2]);
	
	Consumer	*pcons = new Consumer(prdcnt * rptcnt);
	pcons->start();
	
	for(int i = 0; i < prdcnt; ++i){
		Producer *prod = new Producer(*pcons, i, rptcnt);
		prod->start();
	}
	
	
	Thread::waitAll();
	return 0;
}


void Producer::run(){
	for(size_t i = 0; i < cnt; ++i){
		switch(i % BaseMessage::MaxType){
			case BaseMessage::FirstType:
				rcns.push(FirstMessage(idx, i));break;
			case BaseMessage::SecondType:
				rcns.push(SecondMessage(idx, i));break;
			case BaseMessage::ThirdType:
				rcns.push(ThirdMessage(idx, i));break;
			case BaseMessage::FourthType:
				rcns.push(FourthMessage(idx, i));break;
			case BaseMessage::FifthType:
				rcns.push(FifthMessage(idx, i));break;
			case BaseMessage::SixthType:
				rcns.push(SixthMessage(idx, i));break;
			case BaseMessage::SeventhType:
				rcns.push(SeventhMessage(idx, i));break;
			default:
				break;
		}
	}
}

#ifndef USE_CACHE
void Consumer::run(){
	typedef vector<BaseMessage*> 	MessageVectorT;
	MessageVectorT	msgvec;
	msgvec.reserve(1024);
	while(waitcnt){
		{
			Locker<Mutex>	lock(mtx);
			while(msgq.empty()){
				cnd.wait(lock);
			}
			while(msgq.size() && msgvec.size() < msgvec.capacity()){
				msgvec.push_back(msgq.front());
				msgq.pop();
			}
		}
		for(MessageVectorT::const_iterator it(msgvec.begin()); it != msgvec.end(); ++it){
			BaseMessage *pbmsg = *it;
			switch(pbmsg->type){
				case BaseMessage::FirstType:{
					FirstMessage *pmsg = static_cast<FirstMessage*>(pbmsg);
					sum += pmsg->v;
					delete pmsg;
				}break;
					
				case BaseMessage::SecondType:{
					SecondMessage *pmsg = static_cast<SecondMessage*>(pbmsg);
					sum += pmsg->v;
					delete pmsg;
				}break;
				case BaseMessage::ThirdType:{
					ThirdMessage *pmsg = static_cast<ThirdMessage*>(pbmsg);
					sum += pmsg->v;
					delete pmsg;
				}break;
				case BaseMessage::FourthType:{
					FourthMessage *pmsg = static_cast<FourthMessage*>(pbmsg);
					sum += pmsg->v1;
					delete pmsg;
				}break;
				case BaseMessage::FifthType:{
					FifthMessage *pmsg = static_cast<FifthMessage*>(pbmsg);
					sum += pmsg->v1;
					delete pmsg;
				}break;
				case BaseMessage::SixthType:{
					SixthMessage *pmsg = static_cast<SixthMessage*>(pbmsg);
					sum += pmsg->v1;
					delete pmsg;
				}break;
				case BaseMessage::SeventhType:{
					SeventhMessage *pmsg = static_cast<SeventhMessage*>(pbmsg);
					sum += pmsg->v1;
					delete pmsg;
				}break;
				default:break;
			}
			--waitcnt;
		}
		msgvec.clear();
	}
	cout<<"sum "<<sum<<endl;
}
#else
void Consumer::run(){
	typedef vector<BaseMessage*> 	MessageVectorT;
	MessageVectorT	msgvec;
	msgvec.reserve(1024);
	while(waitcnt){
		{
			Locker<Mutex>	lock(mtx);
			for(MessageVectorT::const_iterator it(msgvec.begin()); it != msgvec.end(); ++it){
				char 				*pbuf = reinterpret_cast<char*>(*it);
				const solid::uint	*pidx = reinterpret_cast<const solid::uint*>(pbuf);
				if(*pidx < 4 && bufstks[*pidx].size() < 4*1024){
					bufstks[*pidx].push(pbuf);
				}else{
					delete []pbuf;
				}
			}
			msgvec.clear();
			while(msgq.empty()){
				cnd.wait(lock);
			}
			while(msgq.size() && msgvec.size() < msgvec.capacity()){
				msgvec.push_back(msgq.front());
				msgq.pop();
			}
		}
		for(MessageVectorT::const_iterator it(msgvec.begin()); it != msgvec.end(); ++it){
			BaseMessage *pbmsg = *it;
			switch(pbmsg->type){
				case BaseMessage::FirstType:{
					FirstMessage *pmsg = static_cast<FirstMessage*>(pbmsg);
					sum += pmsg->v;
					pmsg->~FirstMessage();
					cacheSize2Index(*reinterpret_cast<solid::uint*>(pbmsg), sizeof(*pmsg));
				}break;
				case BaseMessage::SecondType:{
					SecondMessage *pmsg = static_cast<SecondMessage*>(pbmsg);
					sum += pmsg->v;
					pmsg->~SecondMessage();
					cacheSize2Index(*reinterpret_cast<solid::uint*>(pbmsg), sizeof(*pmsg));
				}break;
				case BaseMessage::ThirdType:{
					ThirdMessage *pmsg = static_cast<ThirdMessage*>(pbmsg);
					sum += pmsg->v;
					pmsg->~ThirdMessage();
					cacheSize2Index(*reinterpret_cast<solid::uint*>(pbmsg), sizeof(*pmsg));
				}break;
				case BaseMessage::FourthType:{
					FourthMessage *pmsg = static_cast<FourthMessage*>(pbmsg);
					sum += pmsg->v1;
					pmsg->~FourthMessage();
					cacheSize2Index(*reinterpret_cast<solid::uint*>(pbmsg), sizeof(*pmsg));
				}break;
				case BaseMessage::FifthType:{
					FifthMessage *pmsg = static_cast<FifthMessage*>(pbmsg);
					sum += pmsg->v1;
					pmsg->~FifthMessage();
					cacheSize2Index(*reinterpret_cast<solid::uint*>(pbmsg), sizeof(*pmsg));
				}break;
				case BaseMessage::SixthType:{
					SixthMessage *pmsg = static_cast<SixthMessage*>(pbmsg);
					sum += pmsg->v1;
					pmsg->~SixthMessage();
					cacheSize2Index(*reinterpret_cast<solid::uint*>(pbmsg), sizeof(*pmsg));
				}break;
				case BaseMessage::SeventhType:{
					SeventhMessage *pmsg = static_cast<SeventhMessage*>(pbmsg);
					sum += pmsg->v1;
					pmsg->~SeventhMessage();
					cacheSize2Index(*reinterpret_cast<solid::uint*>(pbmsg), sizeof(*pmsg));
				}break;
				default:break;
			}
			--waitcnt;
		}
	}
	cout<<"sum "<<sum<<endl;
}
#endif
