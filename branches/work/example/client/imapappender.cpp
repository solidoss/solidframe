#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <cerrno>
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <deque>
#include <cerrno>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/exception.hpp>
#include <boost/utility.hpp>

#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"

#include "system/cassert.hpp"
#include "system/debug.hpp"
#include "system/directory.hpp"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/timespec.hpp"
#include "system/filedevice.hpp"
//#include "common/utils.h"
#include "writer.hpp"
#include "base64stream.h"
#include "system/common.hpp"
#include "utility/istream.hpp"
#include <fstream>

#include <termios.h>

#include "boost/program_options.hpp"

using namespace std;

namespace fs = boost::filesystem;
using boost::filesystem::path;
using boost::next;
using boost::prior;

enum {BSIZE = 1024};

static SSL_CTX *sslctx = NULL;


struct Params{
	Params(){
		host = "localhost";
		port = "143";
		secure = false;
		maxsz = 10 * 1024 * 1024;
		maxcnt = 50;
	}
	string	host;
	string	port;
	bool	secure;
	string	path;
	string	folder;
	uint32	maxsz;
	uint32	maxcnt;
};

typedef std::pair<std::string*, bool>	DirItemT;

struct DirItemVector: std::deque<DirItemT>{
	void push(const string &_str, bool _isdir){
		push_back(DirItemT(new string(_str), _isdir));
	}
	~DirItemVector(){
		for(const_iterator it(begin()); it != end(); ++it){
			delete it->first;
		}
	}
};

struct DirItemCmp{
	bool operator()(const DirItemT &_i1, const DirItemT &_i2)const{
		if(_i1.second == _i2.second) return (*_i1.first) < (*_i2.first);
		return _i1.second;
	}
};

bool parseArguments(Params &_par, int argc, char *argv[]);
int initOutput(ofstream &_os, const Params &_p, int _cnt, int _piccnt);
bool addFile(ofstream &_os, const Params &_par, const string &_s);
bool doAuthenticate(Writer &_wr, SocketDevice &_sd, SSL *_pssl);
bool doCreateFolder(Writer &_wr, SocketDevice &_sd, SSL *_pssl, const Params &_par);
bool sendOutput(Writer &_wr, SocketDevice &_sd, SSL *_pssl, const Params &_par);
int doneOutput(ofstream &_os);

int main(int argc, char *argv[]){
	Params	p;
	cout<<"OSSL_SOURCE_PATH = "<<OSSL_SOURCE_PATH<<endl;
	if(parseArguments(p, argc, argv)) return 0;
	
	//Initializing OpenSSL
	//------------------------------------------
	SSL_library_init();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();
	
	sslctx = SSL_CTX_new(SSLv23_client_method());
	
	const char *pcertpath = OSSL_SOURCE_PATH"ssl_/certs/A-client.pem";
	cout<<"Client certificate path: "<<pcertpath<<endl;
	if(!sslctx){
		cout<<"failed SSL_CTX_new: "<<ERR_error_string(ERR_get_error(), NULL)<<endl;
		return 0;
	}
	if(!SSL_CTX_load_verify_locations(sslctx, pcertpath, NULL)){
    	cout<<"failed SSL_CTX_load_verify_locations 1 "<<ERR_error_string(ERR_get_error(), NULL)<<endl;;
    	return 0;
	}
	//system("mkdir certs");
	Directory::create("certs");
	
	if(!SSL_CTX_load_verify_locations(sslctx, NULL, "certs")){
		cout<<"failed SSL_CTX_load_verify_locations 2 "<<ERR_error_string(ERR_get_error(), NULL)<<endl;;
		return 0;
	}
	//------------------------------------------
	//done with ssl context stuff
	
	SocketDevice sd;
	
	{
		ResolveData    rd = synchronous_resolve(p.host.c_str(), p.port.c_str());
		if(rd.empty()){
			cout<<"No such address: "<<p.host<<":"<<p.port<<endl;
			return 0;
		}
		
		
		sd.create(rd.begin());
		if(sd.connect(rd.begin())){
			cout<<"Unable to connect to: "<<p.host<<":"<<p.port<<endl;
			return 0;
		}
	}
	{
		SocketAddress sa;
		sd.remoteAddress(sa);
		char host[64];
		char serv[64];
		sa.toString(host, 64, serv, 64);
		cout<<"Successfully connected to: "<<host<<':'<<serv<<endl;
	}
	Writer      wr;
	//do ssl stuffs
	SSL *pssl = SSL_new(sslctx);
	SSL_set_fd(pssl, sd.descriptor());
	int rv = SSL_connect(pssl);
	if(rv <= 0){
		cout<<"error ssl connect"<<endl;
		return 0;
	}
	wr.reinit(sd.descriptor(), pssl);
	
	if(!doAuthenticate(wr, sd, pssl)){
		cout<<"Authentication failed"<<endl;
		return 0;
	}
	cout<<"Successfully authenticated"<<endl;
	//return 0;
	if(!doCreateFolder(wr, sd, pssl, p)){
		cout<<"Unable to create IMAP folder: "<<p.folder<<endl;
		return 0;
	}
	cout<<"Successfully created folder: "<<p.folder<<endl;
	//------------------------------------------
	ofstream tmpf;
	DirItemVector	dirv;
	{
		fs::directory_iterator 	it,end;
		try{
		fs::path pth(p.path/*, fs::native*/);
		it = fs::directory_iterator(pth);
		}catch ( const std::exception & ex ){
			cout<<"iterator exception"<<endl;
			return OK;
		}
		while(it != end){
			dirv.push(it->path().c_str(), is_directory(*it));
			++it;
		}
		std::sort(dirv.begin(), dirv.end(), DirItemCmp());
	}
	int count = 0;
	int piccnt = 0;
	initOutput(tmpf, p, count, piccnt);
	int added = 0;
	cout<<"p.path = "<<p.path<<endl;
	DirItemVector::const_iterator it(dirv.begin());
	while(it != dirv.end()){
// 		cout<<"Adding file "<<*it->first<<" dir = "<<it->second<<endl;
// 		++it;
// 		continue;
		if(it->second){
			++it;
			continue;
		}
		if(added >= p.maxcnt || addFile(tmpf, p, *it->first)){
			//done with the output
			doneOutput(tmpf);
			if(!sendOutput(wr, sd, pssl, p)){
				cout<<"append failed"<<endl;
				return 0;
			}
			++count;
			added = 0;
			initOutput(tmpf, p, count, piccnt);
		}else{
			++added;
		}
		++piccnt;
		++it; 
	}
	cout<<"done"<<endl;
	if(added){
		doneOutput(tmpf);
		tmpf.flush();
		sendOutput(wr, sd, pssl, p);
	}
	tmpf.close();
	return 0;
}

// bool parseArguments(Params &_par, int argc, char *argv[]){
// 	try{
// 		TCLAP::CmdLine cmd("imapappender application", ' ', "0.1");
// 		
// 		TCLAP::ValueArg<std::string> host("a","addr","IMAP server host",false,"localhost","string");
// 		TCLAP::ValueArg<std::string> port("p","port","IMAP server port",false,"143","string");
// 		TCLAP::ValueArg<std::string> path("l","localpath","Path to local folder containing files",true,"","string");
// 		TCLAP::ValueArg<std::string> folder("f","imapfolder","imap folder to push data on",true,"","string");
// 		TCLAP::ValueArg<uint32> maxsz("m","maxsz","per mail maximum size",false,1024 * 1024 * 5,"uint32");
// 		TCLAP::ValueArg<uint32> maxcnt("c","maxcnt","per mail maximum attachement count",false,50,"uint32");
// 		TCLAP::SwitchArg ssl("s","ssl", "Use ssl for communication", false, false);
// 		
// 		cmd.add(host);
// 		cmd.add(port);
// 		cmd.add(path);
// 		cmd.add(folder);
// 		cmd.add(ssl);
// 		cmd.add(maxsz);
// 		cmd.add(maxcnt);
// 		
// 		// Parse the argv array.
// 		cmd.parse( argc, argv );
// 		
// 		_par.host = host.getValue();
// 		_par.port = port.getValue();
// 		_par.path = path.getValue();
// 		_par.folder = folder.getValue();
// 		_par.secure = ssl.getValue();
// 		_par.maxsz = maxsz.getValue();
// 		_par.maxcnt = maxcnt.getValue();
// 		return false;
// 	}catch(TCLAP::ArgException &e){// catch any exceptions
// 		std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
// 		return true;
// 	}
// }

bool parseArguments(Params &_par, int argc, char *argv[]){
	using namespace boost::program_options;
	try{
		options_description desc("ImapAppender application");
		desc.add_options()
			("help,h", "List program options")
			("addr,a", value<string>(&_par.host),"Server address")
			("port,p", value<string>(&_par.port),"Server port")
			("path,l", value<string>(&_par.path),"Local path")
			("folder,f", value<string>(&_par.folder), "Imap folder on server")
			("maxsz,m", value<uint32>(&_par.maxsz)->default_value(1024 * 1024 * 5), "Per mail maximum size")
			("maxcnt,c", value<uint32>(&_par.maxcnt)->default_value(5), "Per mail maximum attachement count")
			("ssl,s",value<bool>(&_par.secure)->default_value(true), "Use ssl")
	/*		("verbose,v", po::value<int>()->implicit_value(1),
					"enable verbosity (optionally specify level)")*/
	/*		("listen,l", po::value<int>(&portnum)->implicit_value(1001)
					->default_value(0,"no"),
					"listen on a port.")
			("include-path,I", po::value< vector<string> >(),
					"include path")
			("input-file", po::value< vector<string> >(), "input file")*/
		;
		variables_map vm;
		store(parse_command_line(argc, argv, desc), vm);
		notify(vm);
		if (vm.count("help")) {
			cout << desc << "\n";
			return true;
		}
		return false;
	}catch(exception& e){
		cout << e.what() << "\n";
		return true;
	}
}

int initOutput(ofstream &_os, const Params &_p, int _cnt, int _piccnt){
	//write the header:
	_os.close();
	_os.clear();
	_os.open("tmp.eml");
	cout<<"is open temp = "<<_os.is_open()<<endl;
	
	_os<<"To:  \"pictures\"\n";
	_os<<"From:  \"pictures\"\n";
	_os<<"Subject: "<<_p.folder<<' '<<_cnt<<' '<<_piccnt<<"\n";
	_os<<"Date: Wed, 21 Jan 2009 19:10:10 +0200\n";
	_os<<"MIME-Version: 1.0\n";
	_os<<"Content-Type: multipart/mixed;\n";
	_os<<"\tboundary=\"----=_NextPart_000_0007_01C97BFB.E8916420\"\n";
	_os<<"X-Priority: 3\n";
	_os<<"X-MSMail-Priority: Normal\n";
	_os<<"X-Unsent: 1\n";
	_os<<"X-MimeOLE: Produced By Microsoft MimeOLE V6.00.2900.3138\n";
	_os<<"\n";
	_os<<"This is a multi-part message in MIME format.\n";
	_os<<"\n";
	_os<<"------=_NextPart_000_0007_01C97BFB.E8916420\n";
	_os<<"Content-Type: multipart/alternative;\n";
    _os<<"\tboundary=\"----=_NextPart_001_0008_01C97BFB.E8916420\"\n";
	_os<<"\n";
	_os<<"\n";
	_os<<"------=_NextPart_001_0008_01C97BFB.E8916420\n";
	_os<<"Content-Type: text/plain;\n";
	_os<<"\tcharset=\"iso-8859-1\"\n";
	_os<<"Content-Transfer-Encoding: quoted-printable\n";
	_os<<"\n";
	_os<<"some text\n\n";
	_os<<"------=_NextPart_001_0008_01C97BFB.E8916420--\n";

	return 0;
}

int  copystream(ostream &_os, istream &_is){
	char buf[1024];
	while(_is.good() && !_is.eof()){
		_is.read(buf, 1024);
		_os.write(buf, _is.gcount());
	}
	return 0;
}
void contentType(string &_o, const string &_i){
	const char *ext = strrchr(_i.c_str(), '.');
	if(!ext) ext = NULL;
	else ++ext;
	if(strcasecmp(ext, "jpg") == 0){
		_o += "image/jpeg";
	}else if(strcasecmp(ext, "gif") == 0){
		_o += "image/gif";
	}else if(strcasecmp(ext, "png") == 0){
		_o += "image/x-png";
	}else if(strcasecmp(ext, "bmp") == 0){
		_o += "image/x-ms-bmp";
	}else if(strcasecmp(ext, "jpe") == 0){
		_o += "image/jpeg";
	}else if(strcasecmp(ext, "jpeg") == 0){
		_o += "image/jpeg";
	}else if(strcasecmp(ext, "pdf") == 0){
		_o += "application/pdf";
	}
}

uint64 size(ifstream &_ifs){
	_ifs.seekg (0, ios::end);
	uint64 length = _ifs.tellg();
	_ifs.seekg (0, ios::beg);
	return length;
}

uint64 size(ofstream &_ofs){
	long pos = _ofs.tellp();
	_ofs.seekp (0, ios::end);
	uint64 length = _ofs.tellp();
	_ofs.seekp (pos, ios::beg);
	return length;
}

bool addFile(ofstream &_os, const Params &_p, const string &_s){
	ifstream ifs;
	ifs.open(_s.c_str());
	
	uint64 ifsize(size(ifs));
	
	if(ifsize >= _p.maxsz){
		cout<<"Skip file "<<_s<<" with size "<<ifsize<<endl;
		return false;
	}else{
		cout<<"Adding file "<<_s<<" with size "<<ifsize<<endl;
	}
	
	const char *name = strrchr(_s.c_str(), '/');
	if(!name) name = _s.c_str();
	else ++name;
	string ctp;
	contentType(ctp, _s);
	
	_os<<"\n\n";
	_os<<"------=_NextPart_000_0007_01C97BFB.E8916420\n";
	_os<<"Content-Type: "<<ctp<<";\n";
    _os<<"\tname=\""<<name<<"\"\n";
	_os<<"Content-Transfer-Encoding: base64\n";
	_os<<"Content-Disposition: attachment;\n";
	_os<<"\tfilename=\""<<name<<"\"\n\n";

	
	cxxtools::Base64ostream b64os(_os);
	copystream(b64os, ifs);
	b64os.end();
	uint64 ofsize(size(_os));
	cout<<"Current output file size: "<<ofsize<<endl;
	if(ofsize >= _p.maxsz) return true;
	return false;
}
int doneOutput(ofstream &_os){
	_os<<"\r\n\r\n------=_NextPart_000_0007_01C97BFB.E8916420--\r\n";
	return 0;
}

int readAtLeast(int _minsz, SSL *_pssl, char *_pb, unsigned _sz);

class FileStream: public InputStream{
public:
	FileStream(FileDevice &_rfd):fd(_rfd){}
	int read(char *_pb, uint32 _sz, uint32 _flags = 0){
		return fd.read(_pb, _sz);
	}
	int64 seek(int64, SeekRef){
		return -1;
	}
private:
	FileDevice &fd;
};

int readFindEither(const char *_f1, const char *_f2, SSL *_pssl, char *_pb, unsigned _sz);

bool sendOutput(Writer &_wr, SocketDevice &_sd, SSL *_pssl, const Params &_p){
// 	ifstream ifs;
// 	ifs.open("tmp.eml");
// 	copystream(cout, ifs);
	FileDevice fd;
	fd.open("tmp.eml", FileDevice::RO);
	FileStream fs(fd);
	cout<<"Sending tmp file of size "<<fd.size()<<endl;
	_wr<<"s3 append \""<<_p.folder<<'\"'<<' '<<lit(&fs, fd.size())<<crlf;
	_wr.flush();
	const int blen = 256;
	char buf[blen];
	int rc = readAtLeast(5, _pssl, buf, blen);
	if(rc > 0){
		cout.write(buf, rc)<<endl;
		rc = readAtLeast(5, _pssl, buf, blen);
		if(rc > 0){
			cout.write(buf, rc)<<endl;
			if(strncasecmp(buf + 3, "OK", 2) == 0){
				return true;
			}
		}else{
			cout<<"connection closed by peer"<<endl;
		}
		
	}
	return false;
}

bool doAuthenticate(Writer &_wr, SocketDevice &_sd, SSL *_pssl){
	string u;
	string p;
	cout<<"User: ";cin>>u;
	termios tio;
	tcgetattr(fileno(stdin), &tio);
	tio.c_lflag &= ~ECHO;
	tcsetattr(fileno(stdin), TCSANOW, &tio);
	cout<<"Pass: ";cin>>p;
	tio.c_lflag |= ECHO;
	tcsetattr(fileno(stdin), TCSANOW, &tio);
	//cout<<"User name: "<<u<<" pass: "<<p<<endl;
	_wr<<"s1 login \""<<u<<"\" \""<<p<<'\"'<<crlf;
	_wr.flush();
	const int blen = 1024;
	char buf[blen];
	int rv = readFindEither("s1 OK", "s1 NO", _pssl, buf, blen);
	if(rv == 0) return true;
	return true;
}
bool doCreateFolder(Writer &_wr, SocketDevice &_sd, SSL *_pssl, const Params &_par){
	_wr<<"s2 create \""<<_par.folder<<"\""<<crlf;
	_wr.flush();
	const int blen = 256;
	char buf[blen];
	int rc = readAtLeast(5, _pssl, buf, blen);
	if(rc > 0){
		cout.write(buf, rc)<<endl;
		if(strncasecmp(buf + 4, "OK", 2) == 0){
			return true;
		}
		return true;
	}
	return false;
}

int readAtLeast(int _minsz, SSL *_pssl, char *_pb, unsigned _sz){
	char *pd = (char*)_pb;
	int rc = 0;
	while(_minsz > 0 && _sz){
		int rv = SSL_read(_pssl, pd, _sz);
		if(rv <= 0){
			rv = SSL_read(_pssl, pd, _sz);//try once again
			if(rv <= 0) return -1;
		}
		_sz -= 0;
		_minsz -= rv;
		pd += rv;
		_sz -= rv;
		rc += rv;
	}
	return rc;
}

int readFindEither(const char *_f1, const char *_f2, SSL *_pssl, char *_pb, unsigned _sz){
	uint32 maxfsz = strlen(_f1);
	if(strlen(_f2) > maxfsz) maxfsz = strlen(_f2);
	char *pd = (char*)_pb;
	uint toread = _sz - 1;
	while(true){
		int rv = SSL_read(_pssl, pd, toread);
		if(rv <= 0){
			rv = SSL_read(_pssl, pd, _sz);//try once again
			if(rv <= 0) return -1;
		}
		pd[rv] = '\0';
		if(strstr(_pb, _f1)) return 0;
		if(strstr(_pb, _f2)) return 1;
		uint tcp = maxfsz;
		if(tcp > rv) tcp = rv;
		memmove(_pb, pd + rv - tcp, tcp);
		pd = _pb + tcp;
		toread = _sz - tcp - 1;
	}
}
