#include <streambuf>
#include <istream>
#include <ostream>
#include <iostream>
#include <string>
#include <cstring>
#include "system/debug.hpp"
#include "system/filedevice.hpp"

using namespace std;
using namespace solid;


class FileBuf: public std::streambuf{
public:
	FileBuf(
		FileDevice &_rdev,
		char* _rbuf = NULL, size_t _rbufcp = 0,
		char* _wbuf = NULL, size_t _wbufcp = 0
	):dev(_rdev), rbuf(_rbuf), rbufcp(_rbufcp), rbufsz(0), wbuf(_wbuf), wbufcp(_wbufcp), wbufsz(0), roff(0), woff(0){
		if(_rbufcp){
			char *end = _rbuf + _rbufcp;
			setg(end, end, end);
		}
		if(_wbufcp){
			char *end = _wbuf + _rbufcp;
			setp(_wbuf, end);
		}
	}
	FileBuf(
		char* _rbuf = NULL, size_t _rbufcp = 0,
		char* _wbuf = NULL, size_t _wbufcp = 0
	): rbuf(_rbuf), rbufcp(_rbufcp), rbufsz(0), wbuf(_wbuf), wbufcp(_wbufcp), wbufsz(0), roff(0), woff(0){
		if(_rbufcp){
			char *end = _rbuf + _rbufcp;
			setg(end, end, end);
		}
		if(_wbufcp){
			char *end = _wbuf + _rbufcp;
			setp(_wbuf, end);
		}
	}
	FileDevice& device(){
		return dev;
	}
	void device(FileDevice &_rdev){
		dev = _rdev;
	}
protected:
	/*virtual*/ streamsize showmanyc();
	/*virtual*/ int_type underflow();
	/*virtual*/ int_type uflow();
	/*virtual*/ int_type pbackfail(int_type _c = traits_type::eof());
	/*virtual*/ int_type overflow(int_type __c = traits_type::eof());
	/*virtual*/ pos_type seekoff(
		off_type _off, ios_base::seekdir _way,
		ios_base::openmode _mode = ios_base::in | ios_base::out
	);
	/*virtual*/ pos_type seekpos(
		pos_type _pos,
		ios_base::openmode _mode = ios_base::in | ios_base::out
	);
	/*virtual*/ int sync();
	///*virtual*/ void imbue(const locale& _loc);
	/*virtual*/ streamsize xsgetn(char_type* _s, streamsize _n);
	/*virtual*/ streamsize xsputn(const char_type* _s, streamsize _n);
private:
	FileDevice		dev;
	char			*rbuf;
	const size_t	rbufcp;
	size_t			rbufsz;
	char			*wbuf;
	const size_t	wbufcp;
	size_t			wbufsz;
	int64			roff;
	int64			woff;
};


/*virtual*/ streamsize FileBuf::showmanyc(){
	idbg("");
	return 0;
}

/*virtual*/ FileBuf::int_type FileBuf::underflow(){
	//idbg("");
	if(rbufcp){
		if (gptr() < egptr()){ // buffer not exhausted
			return traits_type::to_int_type(*gptr());
		}
		//refill rbuf
		idbg("read "<<rbufcp<<" from "<<roff);
		int 	rv = dev.read(rbuf, rbufcp, roff);
		if(rv > 0){
			char	*end = rbuf + rv;
			setg(rbuf, rbuf, end);
			roff += rv;
			rbufsz = rv;
			return traits_type::to_int_type(*gptr());
		}
	}else{
		//very inneficient
		char c;
		int rv = dev.read(&c, 1, roff);
		if(rv == 1){
			return traits_type::to_int_type(c);
		}
	}
	rbufsz = 0;
	return traits_type::eof();
}

/*virtual*/ FileBuf::int_type FileBuf::uflow(){
	//idbg("");
	if(rbufcp){
		return streambuf::uflow();
	}else{
		//very inneficient
		char		c;
		const int	rv = dev.read(&c, 1, roff);
		if(rv == 1){
			++roff;
			return traits_type::to_int_type(c);
		}
	}
	return traits_type::eof();
}

/*virtual*/ FileBuf::int_type FileBuf::pbackfail(int_type _c){
	idbg(""<<_c);
	return traits_type::eof();
}

/*virtual*/ FileBuf::int_type FileBuf::overflow(int_type _c){
	idbg(""<<_c);
	//return traits_type::eof();
	if(wbufcp){
		
	}else{
		const char	c = _c;
		const int	rv = dev.write(&c, 1, woff);
		if(rv == 1){
			++woff;
			return traits_type::to_int_type(c);
		}
	}
	return traits_type::eof();
}

/*virtual*/ FileBuf::pos_type FileBuf::seekoff(
	off_type _off, ios_base::seekdir _way,
	ios_base::openmode _mode
){
	FileBuf::pos_type pos = 0;
	if(_way == ios_base::beg){
		idbg("beg "<<_off);
    }else if(_way == ios_base::end){
		idbg("end "<<_off);
        _off = dev.size() + _off;
    }else{
		idbg("cur "<<_off);
	}
	
	if(_mode & ios_base::in){
		idbg("in "<<_off<<' '<<rbufsz);
		pos = roff;
		if(_way == ios_base::cur){
			pos += _off;
		}else{
			pos = _off;
		}
		if(rbufsz){
			int64	crtbufoff = roff - rbufsz;
			
			idbg("have buffer rbufsz = "<<rbufsz<<" roff = "<<roff<<" crtbufoff = "<<crtbufoff<<" pos = "<<pos);
			
			if(crtbufoff < pos && pos < roff){
				//the requested offset is inside the current buffer
				size_t	bufoff = pos - crtbufoff;
				idbg("bufoff = "<<bufoff);
				setg(rbuf, rbuf + bufoff, rbuf + rbufcp);
			}else{
				idbg("");
				char	*end = rbuf + rbufcp;
				rbufsz = 0;
				roff = pos;
				setg(end, end, end);
			}
		}else{
			idbg("");
			roff = pos;
		}
	}
	if(_mode & ios_base::out){
		idbg("out "<<_off);
		if(_way == ios_base::cur){
			woff += _off;
		}else{
			woff = _off;
		}
		pos = woff;
	}
	return pos;
}

/*virtual*/ FileBuf::pos_type FileBuf::seekpos(
	pos_type _pos,
	ios_base::openmode _mode
){
	idbg(""<<_pos);
	return seekoff(_pos, std::ios_base::beg, _mode);
}

/*virtual*/ int FileBuf::sync(){
	idbg("");
	return 0;
}

// /*virtual*/ void FileBuf::imbue(const locale& _loc){
// 	
// }

/*virtual*/ streamsize FileBuf::xsgetn(char_type* _s, streamsize _n){
	idbg(""<<_n);
	if(rbufcp){
		if (gptr() < egptr()){ // buffer not exhausted
			streamsize sz = egptr() - gptr();
			if(sz > _n){
				sz = _n;
			}
			memcpy(_s, gptr(), sz);
			gbump(sz);
			return sz;
		}
		//read directly in the given buffer
		int 	rv = dev.read(_s, _n, roff);
		if(rv > 0){
			roff += rv;
			size_t	tocopy = rv;
			
			if(tocopy > rbufcp){
				tocopy = rbufcp;
			}
			memcpy(rbuf, _s + rv - tocopy, tocopy);
			
			char	*end = rbuf + rbufcp;
			
			rbufsz = 0;
			setg(end, end, end);
			return rv;
		}
	}else{
		const int	rv = dev.read(_s, _n, roff);
		if(rv > 0){
			roff += rv;
			return rv;
		}
	}
	rbufsz = 0;
	return 0;
}

/*virtual*/ streamsize FileBuf::xsputn(const char_type* _s, streamsize _n){
	idbg(""<<_n);
	if(wbufcp){
		
	}else{
		const int	rv = dev.write(_s, _n, woff);
		if(rv > 0){
			woff += rv;
			return rv;
		}
	}
	return 0;
}


//===========================================================================
//===========================================================================

template <size_t BufCp = 0>
class FileIStream;

template <>
class FileIStream<0>: public std::istream{
public:
	explicit FileIStream(
		FileDevice &_rdev, const size_t _bufcp = 1024
	):	std::istream(),
		pb(_bufcp ? new char[_bufcp] : NULL),
		buf(_rdev, pb, _bufcp){
		rdbuf(&buf);
	}
	
	FileIStream(
		const size_t _bufcp = 1024
	):	std::istream(),
		pb(_bufcp ? new char[_bufcp] : NULL),
		buf(pb, _bufcp)
	{
		rdbuf(&buf);
	}
	~FileIStream(){
		delete []pb;
	}
	FileDevice& device(){
		return buf.device();
	}
	void device(FileDevice &_rdev){
		buf.device(_rdev);
	}
private:
	char	*pb;
	FileBuf	buf;
};


template <size_t BufCp>
class FileIStream: public std::istream{
public:
	explicit FileIStream(FileDevice &_rdev):std::istream(), buf(_rdev, b, BufCp){
		rdbuf(&buf);
	}
	FileIStream():std::istream(), buf(b, BufCp){
		rdbuf(&buf);
	}
	FileDevice& device(){
		return buf.device();
	}
	void device(FileDevice &_rdev){
		buf.device(_rdev);
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
		FileDevice &_rdev,
		const size_t _bufcp = 1024
	):	std::ostream(), pb(_bufcp ? new char[_bufcp] : NULL),
		buf(_rdev, NULL, 0, pb, _bufcp)
	{
		rdbuf(&buf);
	}
	FileOStream(
		const size_t _bufcp = 1024
	):	std::ostream(), pb(_bufcp ? new char[_bufcp] : NULL),
		buf(NULL, 0, pb, _bufcp)
	{
		rdbuf(&buf);
    }
	~FileOStream(){
		delete []pb;
	}
	FileDevice& device(){
		return buf.device();
	}
	void device(FileDevice &_rdev){
		buf.device(_rdev);
	}
private:
	char	*pb;
    FileBuf	buf;
};


template <size_t BufCp>
class FileOStream: public std::ostream{
public:
	explicit FileOStream(FileDevice &_rdev):std::ostream(), buf(_rdev, b, BufCp){
		rdbuf(&buf);
	}
	FileOStream():std::ostream(), buf(b, BufCp){
		rdbuf(&buf);
	}
	FileDevice& device(){
		return buf.device();
	}
	void device(FileDevice &_rdev){
		buf.device(_rdev);
	}
private:
	char		b[BufCp];
	FileBuf		buf;
};


template <size_t RBufCp = 0, size_t WBufCp = 0>
class FileIOStream;

template <>
class FileIOStream<0, 0>: public std::iostream{
public:
	explicit FileIOStream(
		FileDevice &_rdev,
		const size_t _rbufcp = 1024,
		const size_t _wbufcp = 1024
	):	std::iostream(),
		prb(_rbufcp ? new char[_rbufcp] : NULL),
		pwb(_rbufcp ? new char[_wbufcp] : NULL),
		buf(_rdev, prb, _rbufcp, pwb, _wbufcp)
	{
		rdbuf(&buf);
	}
	FileIOStream(
		const size_t _rbufcp = 1024,
		const size_t _wbufcp = 1024
	):	std::iostream(),
		prb(_rbufcp ? new char[_rbufcp] : NULL),
		pwb(_rbufcp ? new char[_wbufcp] : NULL),
		buf(prb, _rbufcp, pwb, _wbufcp)
	{
		rdbuf(&buf);
	}

	~FileIOStream(){
		delete []prb;
		delete []pwb;
	}
	FileDevice& device(){
		return buf.device();
	}
	void device(FileDevice &_rdev){
		buf.device(_rdev);
	}
private:
	char	*prb;
	char	*pwb;
	FileBuf	buf;
};


template <size_t RBufCp, size_t WBufCp>
class FileIOStream: public std::iostream{
public:
    explicit FileIOStream(
		FileDevice &_rdev
	):std::iostream(), buf(_rdev, rb, RBufCp, wb, WBufCp){
		rdbuf(&buf);
    }
    FileIOStream(
	):std::iostream(), buf(rb, RBufCp, wb, WBufCp){
		rdbuf(&buf);
    }
    FileDevice& device(){
		return buf.device();
	}
	void device(FileDevice &_rdev){
		buf.device(_rdev);
	}
private:
	char		rb[RBufCp];
	char		wb[WBufCp];
    FileBuf		buf;
};

int main(int argc, char *argv[]){
	
	Debug::the().levelMask("view");
	Debug::the().moduleMask("any");
	Debug::the().initStdErr(false);
	
	idbg("FileBuf example");
	
	{
		FileDevice	fd;
		if(!fd.open("test.txt", FileDevice::WriteOnlyE | FileDevice::CreateE | FileDevice::TruncateE)){
			idbg("Could not open file for writing");
			return 0;
		}
		FileOStream<0>		ofs(fd, 0);
		
		ofs<<'['<<' '<<"this is the most interesting text that was ever written to a file "<<123456789<<' '<<']'<<endl;
	}
	{
		FileDevice	fd;
		if(!fd.open("test.txt", FileDevice::ReadWriteE)){
			idbg("Could not open file for read-write");
			return 0;
		}
		FileIOStream<0,0>	iofs(0, 0);
		iofs.device(fd);
		iofs.seekp(0, ios_base::end);
		iofs<<'<'<<' '<<"this is another line way more interesting than the above one "<<9876543210<<' '<<'>'<<endl;
		string				line;
		getline(iofs, line);
		cout<<"Read first line: "<<line<<endl;
		getline(iofs, line);
		cout<<"Read second line: "<<line<<endl;
		line += '\n';
		iofs.write(line.data(), line.size());
		iofs.seekg(100);
		iofs.seekp(100);
	}
	{
		FileDevice	fd;
		if(!fd.open("test.txt", FileDevice::ReadOnlyE)){
			idbg("Could not open file for read");
			return 0;
		}
		FileIStream<8>		ifs;
		ifs.device(fd);
		string				line;
		getline(ifs, line);
		cout<<"Read first line: "<<line<<endl;
		getline(ifs, line);
		cout<<"Read second line: "<<line<<endl;
		getline(ifs, line);
		cout<<"Read third line: "<<line<<endl;
		ifs.seekg(10);
		cout<<"seek 10 get char: "<<(int)ifs.peek()<<endl;
		ifs.seekg(12);
		cout<<"seek 12 get char: "<<(int)ifs.peek()<<endl;
	}
	return 0;
}