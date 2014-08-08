#include <iostream>

#include "system/thread.hpp"
#include "system/specific.hpp"
#include "system/atomic.hpp"

using namespace std;
using namespace solid;

ATOMIC_NS::atomic<size_t>	cnscnt(0);
ATOMIC_NS::atomic<size_t>	descnt(0);
bool						runspecific = true;
size_t						startcnt = 100;
size_t						incrementby = 100;
size_t						repeatcnt = 300;

template <size_t S>
struct Data{
	Data(){
		++cnscnt;
	}
	~Data(){
		++descnt;
	}
	uint8 d[S];
};

template <size_t S>
struct SpecificData: SpecificObject{
	SpecificData(){
		++cnscnt;
	}
	~SpecificData(){
		++descnt;
	}
	uint8 d[S];
};



struct RunThread: Thread{
	void run();
	void runSpecific();
	void runNormal();
};

void RunThread::run(){
	
	if(runspecific){
		runSpecific();
	}else{
		runNormal();
	}
}

void RunThread::runSpecific(){
	Specific::prepareThread();
	vector<SpecificData<6>* >		d1vec;
	vector<SpecificData<12>* >		d2vec;
	vector<SpecificData<24>* >		d3vec;
	vector<SpecificData<48>* >		d4vec;
	vector<SpecificData<96>* >		d5vec;
	vector<SpecificData<192>* >		d6vec;
	size_t		crtcnt = startcnt;
	for(size_t i = 0; i < repeatcnt; ++i){
		
		for(size_t j = 0; j < crtcnt; ++j){
			d1vec.push_back(new SpecificData<6>);
			d2vec.push_back(new SpecificData<12>);
			d3vec.push_back(new SpecificData<24>);
			d4vec.push_back(new SpecificData<48>);
			d5vec.push_back(new SpecificData<96>);
			d6vec.push_back(new SpecificData<192>);
		}
		for(size_t j = 0; j < crtcnt; ++j){
			delete d1vec[j];
			delete d2vec[j];
			delete d3vec[j];
			delete d4vec[j];
			delete d5vec[j];
			delete d6vec[j];
		}
		d1vec.clear();
		d2vec.clear();
		d3vec.clear();
		d4vec.clear();
		d5vec.clear();
		d6vec.clear();
		crtcnt += incrementby;
	}
	for(size_t i = 0; i < repeatcnt; ++i){
		
		for(size_t j = 0; j < crtcnt; ++j){
			d1vec.push_back(new SpecificData<6>);
			d2vec.push_back(new SpecificData<12>);
			d3vec.push_back(new SpecificData<24>);
			d4vec.push_back(new SpecificData<48>);
			d5vec.push_back(new SpecificData<96>);
			d6vec.push_back(new SpecificData<192>);
		}
		for(size_t j = 0; j < crtcnt; ++j){
			delete d1vec[j];
			delete d2vec[j];
			delete d3vec[j];
			delete d4vec[j];
			delete d5vec[j];
			delete d6vec[j];
		}
		d1vec.clear();
		d2vec.clear();
		d3vec.clear();
		d4vec.clear();
		d5vec.clear();
		d6vec.clear();
	}
}

void RunThread::runNormal(){
	vector<Data<6>* >		d1vec;
	vector<Data<12>* >		d2vec;
	vector<Data<24>* >		d3vec;
	vector<Data<48>* >		d4vec;
	vector<Data<96>* >		d5vec;
	vector<Data<192>* >		d6vec;
	size_t		crtcnt = startcnt;
	for(size_t i = 0; i < repeatcnt; ++i){
		
		for(size_t j = 0; j < crtcnt; ++j){
			d1vec.push_back(new Data<6>);
			d2vec.push_back(new Data<12>);
			d3vec.push_back(new Data<24>);
			d4vec.push_back(new Data<48>);
			d5vec.push_back(new Data<96>);
			d6vec.push_back(new Data<192>);
		}
		for(size_t j = 0; j < crtcnt; ++j){
			delete d1vec[j];
			delete d2vec[j];
			delete d3vec[j];
			delete d4vec[j];
			delete d5vec[j];
			delete d6vec[j];
		}
		d1vec.clear();
		d2vec.clear();
		d3vec.clear();
		d4vec.clear();
		d5vec.clear();
		d6vec.clear();
		crtcnt += incrementby;
	}
	for(size_t i = 0; i < repeatcnt; ++i){
		
		for(size_t j = 0; j < crtcnt; ++j){
			d1vec.push_back(new Data<6>);
			d2vec.push_back(new Data<12>);
			d3vec.push_back(new Data<24>);
			d4vec.push_back(new Data<48>);
			d5vec.push_back(new Data<96>);
			d6vec.push_back(new Data<192>);
		}
		for(size_t j = 0; j < crtcnt; ++j){
			delete d1vec[j];
			delete d2vec[j];
			delete d3vec[j];
			delete d4vec[j];
			delete d5vec[j];
			delete d6vec[j];
		}
		d1vec.clear();
		d2vec.clear();
		d3vec.clear();
		d4vec.clear();
		d5vec.clear();
		d6vec.clear();
	}
}


int main(int argc, char *argv[]){
	Thread::init();
	runspecific = argv[1] && argv[1][0];
	if(runspecific){
		cout<<"Run with specific cache!"<<endl;
	}else{
		cout<<"Run without specific cache!"<<endl;
	}
	{
		const size_t		thrcnt = atoi(argv[2]);
		for(size_t i = 0; i < thrcnt; ++i){
			RunThread *thr = new RunThread;
			thr->start();
		}
	}
	Thread::waitAll();
	cout<<"Constructor call count: "<<cnscnt<<endl;
	cout<<"Destructor call count : "<<descnt<<endl;
	return 0;
}

