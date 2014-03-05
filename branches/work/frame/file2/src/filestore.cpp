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

void Utf8Controller::prepareOpenFile(
	const char *_inpath,
	PathT &_routpath,
	size_t &_ridx,
	size_t &_rflags
){
	
}

void Utf8Controller::openFile(
	File &_rf, const PathT &_path, size_t _flags, ERROR_NS::error_code &_rerr
){
}

void Utf8Controller::prepareOpenTemp(
	FilePointerT &_uniptr, const uint64 _sz, size_t &_ropenidx, size_t &_rflags
){
	
}

void Utf8Controller::openTemp(
	File &_rf, size_t _openidx, size_t _flags, ERROR_NS::error_code &_rerr
){
	
}

}//namespace file
}//namespace frame
}//namespace solid
