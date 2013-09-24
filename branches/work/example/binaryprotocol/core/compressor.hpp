#ifndef EXAMPLE_BINARY_COMPRESSOR_HPP
#define EXAMPLE_BINARY_COMPRESSOR_HPP

#include "system/common.hpp"

struct Compressor{
	Compressor(const size_t _cp);
	~Compressor();
	const size_t reservedSize()const;
	
	bool shouldCompress(const size_t _sz)const;
	bool compress(char *_pdest, size_t &_destsz, const char *_psrc, const size_t _srcsz);
	bool decompress(char *_pdest, size_t &_destsz, const char *_psrc, const size_t _srcsz);
private:
	struct Data;
	Data &d;
};

#endif
