#include <streambuf>
#include <istream>
#include <ostream>
#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include "system/debug.hpp"
#include "system/filedevice.hpp"
#include "system/cassert.hpp"

using namespace std;
using namespace solid;


class FileBuf: public std::streambuf{
public:
	FileBuf(
		FileDevice &_rdev,
		char* _buf = NULL, size_t _bufcp = 0
	):dev(_rdev), buf(_buf), bufcp(_bufcp), rbufsz(0), off(0){
		if(_bufcp){
			char *end = _buf + _bufcp;
			setg(end, end, end);
			setp(_buf, end);
		}
	}
	FileBuf(
		char* _buf = NULL, size_t _bufcp = 0
	): buf(_buf), bufcp(_bufcp), rbufsz(0), off(0){
		if(_bufcp){
			char *end = _buf + _bufcp;
			setg(end, end, end);
			setp(_buf, end);
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
	int writeAll(const char *_s, size_t _n);
	bool flushWrite();
private:
	FileDevice		dev;
	char			*buf;
	const size_t	bufcp;
	size_t			rbufsz;
	int64			off;
};


/*virtual*/ streamsize FileBuf::showmanyc(){
	idbg("");
	return 0;
}

/*virtual*/ FileBuf::int_type FileBuf::underflow(){
	//idbg("");
	if(bufcp){
		idbg("goffset = "<<(gptr() - buf)<<" end = "<<(egptr() - buf));
		idbg("poffset = "<<(pptr() - buf)<<" end = "<<(epptr() - buf));
		if(gptr() < egptr()){ // buffer not exhausted
			return traits_type::to_int_type(*gptr());
		}
		//refill rbuf
		idbg("read "<<bufcp<<" from "<<off);
		int 	rv = dev.read(buf, bufcp, off);
		if(rv > 0){
			char	*end = buf + rv;
			setg(buf, buf, end);
			off += rv;
			rbufsz = rv;
			return traits_type::to_int_type(*gptr());
		}
	}else{
		//very inneficient
		char c;
		int rv = dev.read(&c, 1, off);
		if(rv == 1){
			return traits_type::to_int_type(c);
		}
	}
	rbufsz = 0;
	return traits_type::eof();
}

/*virtual*/ FileBuf::int_type FileBuf::uflow(){
	//idbg("");
	if(bufcp){
		return streambuf::uflow();
	}else{
		//very inneficient
		char		c;
		const int	rv = dev.read(&c, 1, off);
		if(rv == 1){
			++off;
			return traits_type::to_int_type(c);
		}
	}
	return traits_type::eof();
}

/*virtual*/ FileBuf::int_type FileBuf::pbackfail(int_type _c){
	idbg(""<<_c);
	return traits_type::eof();
}

int FileBuf::writeAll(const char *_s, size_t _n){
	int wcnt = 0;
	int	rv;
	do{
		int rv = dev.write(_s, _n, off);
		if(rv > 0){
			_s += rv;
			_n -= rv;
			wcnt += rv;
		}else{
			wcnt = 0;
		}
	}while(rv > 0 && _n != 0);
	return wcnt;
}

/*virtual*/ FileBuf::int_type FileBuf::overflow(int_type _c){
	idbg(""<<_c);
	if(bufcp){
		idbg("goffset = "<<(gptr() - buf)<<" end = "<<(egptr() - buf));
		idbg("poffset = "<<(pptr() - buf)<<" end = "<<(epptr() - buf));
		if(pptr() < epptr()){
			*pptr() = _c;
			return _c;
		}
		
		if(flushWrite()){
			return _c;
		}
	}else{
		const char	c = _c;
		const int	rv = dev.write(&c, 1, off);
		if(rv == 1){
			++off;
			return traits_type::to_int_type(c);
		}
	}
	return traits_type::eof();
}

/*virtual*/ FileBuf::pos_type FileBuf::seekoff(
	off_type _off, ios_base::seekdir _way,
	ios_base::openmode _mode
){
	idbg("goffset = "<<(gptr() - buf)<<" end = "<<(egptr() - buf));
	idbg("poffset = "<<(pptr() - buf)<<" end = "<<(epptr() - buf));
	cassert(pptr() == gptr());
	
	if(bufcp){
		int64 newoff = 0;
		if(_way == ios_base::beg){
		}else if(_way == ios_base::end){
			newoff = off + (pptr() - pbase());
			int64 devsz = dev.size();
			if(newoff <= devsz){
				newoff = devsz;
			}
		}else{//cur
			newoff = off;
			newoff += (gptr() - eback());
			
		}
		newoff += _off;

		if(newoff < (off + (bufcp))){
			
		}
		return newoff;
	}else{
		if(_way == ios_base::beg){
			off = _off;
		}else if(_way == ios_base::end){
			off = dev.size() + _off;
		}else{//cur
			off += _off;
		}
		return off;
	}
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
	if(pbase() < pptr()){
		flushWrite();
	}
	dev.flush();
	return 0;
}

// /*virtual*/ void FileBuf::imbue(const locale& _loc){
// 	
// }

/*virtual*/ streamsize FileBuf::xsgetn(char_type* _s, streamsize _n){
	idbg(""<<_n);
	if(bufcp){
		idbg("goffset = "<<(gptr() - buf)<<" end = "<<(egptr() - buf));
		idbg("poffset = "<<(pptr() - buf)<<" end = "<<(epptr() - buf));
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
		int 	rv = dev.read(_s, _n, off);
		if(rv > 0){
			off += rv;
			size_t	tocopy = rv;
			
			if(tocopy > bufcp){
				tocopy = bufcp;
			}
			memcpy(buf, _s + rv - tocopy, tocopy);
			
			char	*end = buf + bufcp;
			
			rbufsz = 0;
			setg(end, end, end);
			return rv;
		}
	}else{
		const int	rv = dev.read(_s, _n, off);
		if(rv > 0){
			off += rv;
			return rv;
		}
	}
	rbufsz = 0;
	return 0;
}

bool FileBuf::flushWrite(){
	size_t towrite = pptr() - pbase();
	int rv = writeAll(buf, towrite);
	if(rv == towrite){
		off += towrite;
		char *end = buf + bufcp;
		setp(buf, end);
	}else{
		return false;
	}
	return true;
}

/*virtual*/ streamsize FileBuf::xsputn(const char_type* _s, streamsize _n){
	idbg(""<<_n);
	//updateReadBuffer(_s, _n);
	if(bufcp){
		idbg("goffset = "<<(gptr() - buf)<<" end = "<<(egptr() - buf));
		idbg("poffset = "<<(pptr() - buf)<<" end = "<<(epptr() - buf));
		const streamsize	sz = _n;
		size_t				wleftsz = epptr() - pptr();
		size_t				towrite = _n;
		
		if(wleftsz < towrite){
			towrite = wleftsz;
		}
		memcpy(pptr(), _s, towrite);
		pbump(towrite);
		_n -= towrite;
		_s += towrite;
		if(_n != 0){
			if(!flushWrite()){
				return 0;
			}
			if(_n <= bufcp/2){
				memcpy(pptr(), _s, _n);
				pbump(_n);
				setg(buf, pptr(), epptr());
				return sz;
			}else{
				size_t towrite = _n;
				int rv = writeAll(_s, towrite);
				if(rv == towrite){
					off += rv;
					char *end = buf + bufcp;
					setp(buf, end);
					setg(buf, pptr(), epptr());
					return sz;
				}else{
					return 0;
				}
			}
		}
		setg(buf, pptr(), epptr());
		return sz;
	}else{
		const int	rv = dev.write(_s, _n, off);
		if(rv > 0){
			off += rv;
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
		buf(_rdev, pb, _bufcp)
	{
		rdbuf(&buf);
	}
	FileOStream(
		const size_t _bufcp = 1024
	):	std::ostream(), pb(_bufcp ? new char[_bufcp] : NULL),
		buf(pb, _bufcp)
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


template <size_t BufCp = 0>
class FileIOStream;

template <>
class FileIOStream<0>: public std::iostream{
public:
	explicit FileIOStream(
		FileDevice &_rdev,
		const size_t _bufcp = 1024
	):	std::iostream(), pb(_bufcp ? new char[_bufcp] : NULL),
		buf(_rdev, pb, _bufcp)
	{
		rdbuf(&buf);
	}
	FileIOStream(
		const size_t _bufcp = 1024
	):	std::iostream(), pb(_bufcp ? new char[_bufcp] : NULL),
		buf(pb, _bufcp)
	{
		rdbuf(&buf);
    }
	~FileIOStream(){
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
class FileIOStream: public std::iostream{
public:
	explicit FileIOStream(FileDevice &_rdev):std::iostream(), buf(_rdev, b, BufCp){
		rdbuf(&buf);
	}
	FileIOStream():std::iostream(), buf(b, BufCp){
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

int main(int argc, char *argv[]){
	
	Debug::the().levelMask("view");
	Debug::the().moduleMask("any");
	Debug::the().initStdErr(false);
	
	idbg("FileBuf example");
	
	if(0){
		FileDevice	fd;
		if(!fd.open("test.txt", FileDevice::WriteOnlyE | FileDevice::CreateE | FileDevice::TruncateE)){
			idbg("Could not open file for writing");
			return 0;
		}
		FileOStream<0>		ofs(fd, 0);
		
		ofs<<'['<<' '<<"this is the most interesting text that was ever written to a file "<<123456789<<' '<<']'<<endl;
	}
	if(0){
		FileDevice	fd;
		if(!fd.open("test.txt", FileDevice::ReadWriteE)){
			idbg("Could not open file for read-write");
			return 0;
		}
		FileIOStream<8>	iofs/*(0, 0)*/;
		iofs.device(fd);
		iofs.seekp(0, ios_base::end);
		iofs<<'<'<<' '<<"this is another line way more interesting than the above one "<<9876543210<<' '<<'>'<<endl;
		string				line;
		getline(iofs, line);
		cout<<"Read first line: "<<line<<endl;
		getline(iofs, line);
		cout<<"Read second line: "<<line<<endl;
		line += '\n';
		iofs.write(line.data(), line.size()).flush();
		iofs.seekg(100);
		iofs.seekp(100);
	}
	if(0){
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
	if(1){
		
		fstream	fs;
		fs.open("test_1.txt", ios::out);
		fs.close();
		fs.open("test_1.txt");
		fs.seekp(10);
		fs<<"this is the most interesting text that was ever written to a file\n";
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		
		fs.seekg(0);
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		char c;
		fs>>c;
		cout<<"c = "<<(int)c<<endl;
		
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		
		fs.seekp(10);
		fs<<"this is another line way more interesting than the above one\n";
		
		
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		
		fs>>c;
		cout<<"c = "<<c<<endl;
		
		char buf[128];
		fs.seekg(8);
		
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		
		fs.read(buf, 8);
		cout<<"Read("<<fs.gcount()<<"): ";
		cout.write(buf, fs.gcount());
		cout<<endl;
		cout<<"Done STD"<<endl;
	}
	if(1){
		FileDevice	fd;
		if(!fd.open("test_2.txt", FileDevice::ReadWriteE | FileDevice::CreateE | FileDevice::TruncateE)){
			idbg("Could not open file for read-write");
			return 0;
		}
		FileIOStream<1024>	fs/*(0, 0)*/;
		fs.device(fd);
		fs<<"this is the most interesting text that was ever written to a file\n";
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		
		fs.seekg(0);
		char c;
		fs>>c;
		cout<<"c = "<<c<<endl;
		
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		
		fs.seekp(0);
		fs<<"this is another line way more interesting than the above one\n";
		
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		
		fs>>c;
		cout<<"c = "<<c<<endl;
		
		char buf[128];
		fs.seekg(8);
		
		cout<<"tellg = "<<fs.tellg()<<endl;
		cout<<"tellp = "<<fs.tellp()<<endl;
		fs.read(buf, 8);
		cout<<"Read("<<fs.gcount()<<"): ";
		cout.write(buf, fs.gcount());
		cout<<endl;
	}
	return 0;
}