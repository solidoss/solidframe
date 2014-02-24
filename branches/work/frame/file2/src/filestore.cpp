#include "frame/file2/filestore.hpp"

namespace solid{
namespace frame{
namespace file{

struct Utf8Controller::Data{
	Utf8Configuration	cfg;
	TempConfiguration	tempcfg;
	
	Data(const Utf8Configuration &_rcfg, const TempConfiguration &_rtempcfg):cfg(_rcfg), tempcfg(_rtempcfg){}
};

Utf8Controller::Utf8Controller(const Utf8Configuration &_rutf8cfg, const TempConfiguration &_rtempcfg):d(*(new Data(_rutf8cfg, _rtempcfg))){}
Utf8Controller::~Utf8Controller(){
	delete &d;
}

void Utf8Controller::insertPath(const char *_inpath, PathT &_routpath, size_t &_ridx){
	
}

bool Utf8Controller::open(File &_rf, const PathT &_path, size_t _flags, ERROR_NS::error_code &_rerr){
	return false;
}


}//namespace file
}//namespace frame
}//namespace solid
