#include <deque>
#include <vector>
#include <map>

#include "system/cassert.hpp"
#include "algorithm/serialization/typemapper.hpp"
#include "algorithm/serialization/idtypemap.hpp"
namespace serialization{
//================================================================
struct TypeMapper::Data{
	typedef std::vector<BaseTypeMap*> TypeMapVectorTp;
	Data();
	ulong 			sercnt;
	TypeMapVectorTp	tmvec;
};

TypeMapper::Data::Data():sercnt(0){}

TypeMapper::TypeMapper():d(*(new Data)){
}

TypeMapper::~TypeMapper(){
	delete &d;
}

/*static*/ unsigned TypeMapper::newMapId(){
	static unsigned d(-1);
	return ++d;
}

/*static*/ unsigned TypeMapper::newSerializerId(){
	static unsigned d(-1);
	return ++d;
}

void TypeMapper::doRegisterMap(unsigned _id, BaseTypeMap *_pbm){
	cassert(_id == d.tmvec.size());
	d.tmvec.push_back(_pbm);
}

void TypeMapper::doMap(FncTp _pf, unsigned _pos, const char *_name){
	for(Data::TypeMapVectorTp::const_iterator it(d.tmvec.begin()); it != d.tmvec.end(); ++it){
		(*it)->insert(_pf, _pos, _name, d.sercnt);
	}
}

void TypeMapper::serializerCount(unsigned _cnt){
	d.sercnt = _cnt + 1;
}

BaseTypeMap &TypeMapper::getMap(unsigned _id){
	return *d.tmvec[_id];
}

//================================================================

BaseTypeMap::~BaseTypeMap(){
}

//================================================================

struct IdTypeMap::Data{
	typedef std::deque<IdTypeMap::FncTp>	FncVectorTp;
	typedef std::map<const char*, uint32>	NameMapTp;
	NameMapTp	nmap;
	FncVectorTp	fncvec;
};

IdTypeMap::IdTypeMap():d(*(new Data)){
}

IdTypeMap::~IdTypeMap(){
	delete &d;
}

BaseTypeMap::FncTp IdTypeMap::parseTypeIdDone(const std::string &_rstr, ulong _serid){
	const ulong *const pu = reinterpret_cast<const ulong*>(_rstr.data());
	cassert(*pu < d.fncvec.size());
	cassert(d.fncvec[*pu]);
	return d.fncvec[*pu];
}

/*virtual*/ void IdTypeMap::insert(FncTp _pf, unsigned _pos, const char *_name, unsigned _maxcnt){
	uint32 sz = d.fncvec.size();
	//TODO: its a little more complicated than it seems
	sz += _pos;
	d.fncvec[sz] = _pf;
	d.nmap[_name] = sz/_maxcnt;
}

ulong &IdTypeMap::getFunction(FncTp &_rpf, const char *_name, std::string &_rstr, ulong _serid){
	Data::NameMapTp::const_iterator it(d.nmap.find(_name));
	cassert(it != d.nmap.end());
	ulong *pu = reinterpret_cast<ulong*>(const_cast<char*>(_rstr.data()));
	*pu = it->second + _serid;
	_rpf = d.fncvec[it->second + _serid];
	cassert(_rpf != NULL);
	return *pu;
}
//================================================================
}//namespace serialization
