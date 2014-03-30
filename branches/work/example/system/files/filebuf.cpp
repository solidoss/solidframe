#include <streambuf>
#include <istream>
#include <ostream>
#include <iostream>
#include <string>
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
	):dev(_rdev), rbuf(_rbuf), rbufcp(_rbufcp), wbuf(_wbuf), wbufcp(_wbufcp), roff(0), woff(0){
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
	): rbuf(_rbuf), rbufcp(_rbufcp), wbuf(_wbuf), wbufcp(_wbufcp), roff(0), woff(0){
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
	size_t			rbufcp;
	char			*wbuf;
	size_t			wbufcp;
	int64			roff;
	int64			woff;
};


/*virtual*/ streamsize FileBuf::showmanyc(){
	return 0;
}

/*virtual*/ FileBuf::int_type FileBuf::underflow(){
	return traits_type::eof();
}

/*virtual*/ FileBuf::int_type FileBuf::pbackfail(int_type _c){
	return traits_type::eof();
}

/*virtual*/ FileBuf::int_type FileBuf::overflow(int_type _c){
	return traits_type::eof();
}

/*virtual*/ FileBuf::pos_type FileBuf::seekoff(
	off_type _off, ios_base::seekdir _way,
	ios_base::openmode _mode
){
	FileBuf::pos_type pos = 0;
	if(_way == ios_base::beg){
    }else if(_way == ios_base::end){
        _off = dev.size() + _off;
    }else{
		
	}
	if(_mode & ios_base::in){
		if(_way == ios_base::cur){
			roff += _off;
		}else{
			roff = _off;
		}
		pos = roff;
	}
	if(_mode & ios_base::out){
		
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
	return seekoff(_pos, std::ios_base::beg, _mode);
}

/*virtual*/ int FileBuf::sync(){
	return 0;
}

// /*virtual*/ void FileBuf::imbue(const locale& _loc){
// 	
// }

/*virtual*/ streamsize FileBuf::xsgetn(char_type* _s, streamsize _n){
	return 0;
}

/*virtual*/ streamsize FileBuf::xsputn(const char_type* _s, streamsize _n){
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
		FileOStream<8>		ofs(fd);
		
		ofs<<'['<<' '<<"this is the most interesting text that was ever written to a file"<<' '<<']'<<endl;
	}
	{
		FileDevice	fd;
		if(!fd.open("test.txt", FileDevice::ReadWriteE)){
			idbg("Could not open file for read-write");
			return 0;
		}
		FileIOStream<8,8>	iofs;
		iofs.device(fd);
		iofs.seekp(0, ios_base::end);
		iofs<<'<'<<' '<<"this is another line way more interesting than the above one"<<' '<<'>'<<endl;
		string				line;
		getline(iofs, line);
		cout<<"Read first line: "<<line<<endl;
		getline(iofs, line);
		cout<<"Read second line: "<<line<<endl;
	}
	{
		FileDevice	fd;
		if(!fd.open("test.txt", FileDevice::ReadOnlyE)){
			idbg("Could not open file for read");
			return 0;
		}
		FileIStream<>		ifs(0);
		ifs.device(fd);
		string				line;
		getline(ifs, line);
		cout<<"Read first line: "<<line<<endl;
		getline(ifs, line);
		cout<<"Read second line: "<<line<<endl;
	}
	return 0;
}