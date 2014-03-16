#include "frame/file2/filestore.hpp"

namespace solid{
namespace frame{
namespace file{

struct Utf8Controller::Data{
	Utf8Configuration	cfg;
	TempConfiguration	tempcfg;
	
	Data(
		const Utf8Configuration &_rcfg,
		const TempConfiguration &_rtempcfg
	):cfg(_rcfg), tempcfg(_rtempcfg){}
};

Utf8Controller::Utf8Controller(
	const Utf8Configuration &_rutf8cfg,
	const TempConfiguration &_rtempcfg
):d(*(new Data(_rutf8cfg, _rtempcfg))){}

Utf8Controller::~Utf8Controller(){
	delete &d;
}

bool Utf8Controller::prepareFileIndex(
	Utf8OpenCommandBase &_rcmd, size_t &_ridx, size_t &_rflags, ERROR_NS::error_code &_rerr
){
	//find _rcmd.inpath file and set _rcmd.outpath
	//if found set _ridx and return true
	//else return false
	return false;
}

void Utf8Controller::prepareFilePointer(
	Utf8OpenCommandBase &_rcmd, FilePointerT &_rptr, size_t &_rflags, ERROR_NS::error_code &_rerr
){
	//just do map[_rcmd.outpath] = _rptr.uid().first
}

void Utf8Controller::openFile(Utf8OpenCommandBase &_rcmd, FilePointerT &_rptr, ERROR_NS::error_code &_rerr){
	//use _rcmd.outpath to acctually open the file
	//set _rerr accordingly
}

void Utf8Controller::prepareTempIndex(
	CreateTempCommandBase &_rcmd, size_t &_rflags, ERROR_NS::error_code &_rerr
){
	/*
	if the requested size is greater than what we can offer set _rerr and return
	if we can deliver right now - store in cmd the information about the storage where we have available
		space and return
	IDEA:
		Keep for every storage an value waitcount
		if a requested temp could not be delivered right now, increment the waitcount for every storage that applies
			(based on _createflags and on the requested size - the requested size shoud be < what the storage can offer)
			push the FilePointer in a waitqueue
		
		for a new request, although there are waiting requests, we see if we can deliver from a storage with waitcount == 0
			and enough free space
			
		when a request from waitqueue is delivered/popped, decrement the waitcount for all the storages that apply
	*/
}

bool Utf8Controller::prepareTempPointer(
	CreateTempCommandBase &_rcmd, FilePointerT &_rptr, size_t &_rflags, ERROR_NS::error_code &_rerr
){
	//if prepareTempIndex was able to find a free storage, we initiate *(_rptr) accordingly and return true
	//otherwise we store/queue the pointer (eventually, in a stripped down version) and return false
	return false;
}

void Utf8Controller::clear(File &_rf, const size_t _idx){
	
}

}//namespace file
}//namespace frame
}//namespace solid
