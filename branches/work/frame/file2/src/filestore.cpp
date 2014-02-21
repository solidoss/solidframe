#include "frame/file2/filestore.hpp"

namespace solid{
namespace frame{
namespace file{

struct Controller::Data{
	Configuration cfg;
	
	Data(const Configuration _rcfg):cfg(_rcfg){}
};

Controller::Controller(const Configuration &_rcfg):d(*(new Data(_rcfg))){}
Controller::~Controller(){
	delete &d;
}

void Store::insertPath(const char *_inpath, std::string &_outpath, size_t &_ridx){
	
}



}//namespace file
}//namespace frame
}//namespace solid
