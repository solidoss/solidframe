// frame/file/filestream.hpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_FRAME_FILE_STREAM_HPP
#define SOLID_FRAME_FILE_STREAM_HPP

#include "frame/file/filestore.hpp"
#include <streambuf>
#include <istream>
#include <ostream>
#include <iostream>

namespace solid{
namespace frame{
namespace file{

class FileBuf: public std::streambuf{
public:
	FileBuf(
		FilePointerT &_rptr,
		char* _buf = NULL, size_t _bufcp = 0
	);
	FileBuf(
		char* _buf = NULL, size_t _bufcp = 0
	);
	~FileBuf();
	FilePointerT& device();
	void device(FilePointerT &_rptr);
protected:
	/*virtual*/ std::streamsize showmanyc();
	/*virtual*/ int_type underflow();
	/*virtual*/ int_type uflow();
	/*virtual*/ int_type pbackfail(int_type _c = traits_type::eof());
	/*virtual*/ int_type overflow(int_type __c = traits_type::eof());
	/*virtual*/ pos_type seekoff(
		off_type _off, std::ios_base::seekdir _way,
		std::ios_base::openmode _mode = std::ios_base::in | std::ios_base::out
	);
	/*virtual*/ pos_type seekpos(
		pos_type _pos,
		std::ios_base::openmode _mode = std::ios_base::in | std::ios_base::out
	);
	/*virtual*/ int sync();
	///*virtual*/ void imbue(const locale& _loc);
	/*virtual*/ std::streamsize xsgetn(char_type* _s, std::streamsize _n);
	/*virtual*/ std::streamsize xsputn(const char_type* _s, std::streamsize _n);
private:
	bool hasBuf()const{
		return bufcp != 0;
	}
	bool hasPut()const{
		return pptr() != NULL;
	}
	bool hasGet()const{
		return gptr() != egptr();
	}
	int writeAll(const char *_s, size_t _n);
	void resetBoth(){
		char *end = buf + bufcp;
		setg(end, end, end);
		setp(buf, end - 1);
	}
	void resetGet(){
		char *end = buf + bufcp;
		setg(end, end, end);
	}
	void resetPut(){
		char *end = buf + bufcp - 1;
		setp(buf, end);
	}
	bool flushPut();
private:
	FilePointerT	dev;
	char			*buf;
	const size_t	bufcp;
	int64			off;
};


template <size_t BufCp = 0>
class FileIStream;

template <>
class FileIStream<0>: public std::istream{
public:
	explicit FileIStream(
		FilePointerT &_rdev, const size_t _bufcp = 0
	):	std::istream(NULL),
		pb(_bufcp ? new char[_bufcp] : NULL),
		buf(_rdev, pb, _bufcp){
		rdbuf(&buf);
	}
	
	FileIStream(
		const size_t _bufcp = 0
	):	std::istream(NULL),
		pb(_bufcp ? new char[_bufcp] : NULL),
		buf(pb, _bufcp)
	{
		rdbuf(&buf);
	}
	~FileIStream(){
		delete []pb;
	}
	FilePointerT& device(){
		return buf.device();
	}
	void device(FilePointerT &_rdev){
		buf.device(_rdev);
	}
	void close(){
		buf.pubsync();
		buf.device().clear();
	}
private:
	char	*pb;
	FileBuf	buf;
};


template <size_t BufCp>
class FileIStream: public std::istream{
public:
	explicit FileIStream(FilePointerT &_rdev):std::istream(NULL), buf(_rdev, b, BufCp){
		rdbuf(&buf);
	}
	FileIStream():std::istream(NULL), buf(b, BufCp){
		rdbuf(&buf);
	}
	FilePointerT& device(){
		return buf.device();
	}
	void device(FilePointerT &_rdev){
		buf.device(_rdev);
	}
	void close(){
		buf.pubsync();
		buf.device().clear();
	}
private:
	char		b[BufCp];
	FileBuf		buf;
};


template <size_t BufCp = 0>
class FileOStream;

template <>
class FileOStream<0>: public std::ostream{
public:
	explicit FileOStream(
		FilePointerT &_rdev,
		const size_t _bufcp = 0
	):	std::ostream(), pb(_bufcp ? new char[_bufcp] : NULL),
		buf(_rdev, pb, _bufcp)
	{
		rdbuf(&buf);
	}
	FileOStream(
		const size_t _bufcp = 0
	):	std::ostream(), pb(_bufcp ? new char[_bufcp] : NULL),
		buf(pb, _bufcp)
	{
		rdbuf(&buf);
    }
	~FileOStream(){
		delete []pb;
	}
	FilePointerT& device(){
		return buf.device();
	}
	void device(FilePointerT &_rdev){
		buf.device(_rdev);
	}
	void close(){
		buf.pubsync();
		buf.device().clear();
	}
private:
	char	*pb;
    FileBuf	buf;
};


template <size_t BufCp>
class FileOStream: public std::ostream{
public:
	explicit FileOStream(FilePointerT &_rdev):std::ostream(), buf(_rdev, b, BufCp){
		rdbuf(&buf);
	}
	FileOStream():std::ostream(), buf(b, BufCp){
		rdbuf(&buf);
	}
	FilePointerT& device(){
		return buf.device();
	}
	void device(FilePointerT &_rdev){
		buf.device(_rdev);
	}
	void close(){
		buf.pubsync();
		buf.device().clear();
	}
private:
	char		b[BufCp];
	FileBuf		buf;
};


template <size_t BufCp = 0>
class FileIOStream;

template <>
class FileIOStream<0>: public std::iostream{
public:
	explicit FileIOStream(
		FilePointerT &_rdev,
		const size_t _bufcp = 0
	):	std::iostream(NULL), pb(_bufcp ? new char[_bufcp] : NULL),
		buf(_rdev, pb, _bufcp)
	{
		rdbuf(&buf);
	}
	FileIOStream(
		const size_t _bufcp = 0
	):	std::iostream(NULL), pb(_bufcp ? new char[_bufcp] : NULL),
		buf(pb, _bufcp)
	{
		rdbuf(&buf);
    }
	~FileIOStream(){
		delete []pb;
	}
	FilePointerT& device(){
		return buf.device();
	}
	void device(FilePointerT &_rdev){
		buf.device(_rdev);
	}
	void close(){
		buf.pubsync();
		buf.device().clear();
	}
private:
	char	*pb;
    FileBuf	buf;
};


template <size_t BufCp>
class FileIOStream: public std::iostream{
public:
	explicit FileIOStream(FilePointerT &_rdev):std::iostream(NULL), buf(_rdev, b, BufCp){
		rdbuf(&buf);
	}
	FileIOStream():std::iostream(NULL), buf(b, BufCp){
		rdbuf(&buf);
	}
	FilePointerT& device(){
		return buf.device();
	}
	void device(FilePointerT &_rdev){
		buf.device(_rdev);
	}
	void close(){
		buf.pubsync();
		buf.device().clear();
	}
private:
	char		b[BufCp];
	FileBuf		buf;
};

}//namespace file
}//namespace frame
}//namespace solid


#endif
