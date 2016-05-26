#include "example/binaryprotocol/core/compressor.hpp"
#include "quicklz.h"

struct Compressor::Data{
	Data():ressz(400){}
	qlz_state_compress		ctx_compress;
	qlz_state_decompress	ctx_decompress;
	const size_t			ressz;
};

Compressor::Compressor(const size_t _cp):d(*(new Data)){
	
}
Compressor::~Compressor(){
	delete &d;
}

bool Compressor::shouldCompress(const size_t _sz)const{
	return _sz >= 512;
}
const size_t Compressor::reservedSize()const{
	return d.ressz;
}

bool Compressor::compress(char *_pdest, size_t &_destsz, const char *_psrc, const size_t _srcsz){
	_destsz = qlz_compress(_psrc, _pdest, _srcsz, &d.ctx_compress);
	return true;
}
bool Compressor::decompress(char *_pdest, size_t &_destsz, const char *_psrc, const size_t _srcsz){
	_destsz = qlz_decompress(_psrc, _pdest, &d.ctx_decompress);
	return true;
}
