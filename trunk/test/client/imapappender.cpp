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
#include "system/socketaddress.hpp"
#include "system/thread.hpp"
#include "system/mutex.hpp"
#include "system/timespec.hpp"
//#include "common/utils.h"
#include "writer.hpp"
#include "base64stream.h"
#include "system/common.hpp"
#include <fstream>

#include "tclap/CmdLine.h"
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
	}
	string	host;
	string	port;
	bool	secure;
	string	path;
	string	folder;
	uint32	maxsz;
};

bool parseArguments(Params &_par, int argc, char *argv[]);
int initOutput(ofstream &_os, const Params &_p, int _cnt);
bool addFile(ofstream &_os, const Params &_par, const string &_s);
int sendOutput();
int doneOutput();

int main(int argc, char *argv[]){
	Params	p;
	if(parseArguments(p, argc, argv)) return 0;
	ofstream tmpf;
	fs::directory_iterator 	it,end;
	try{
	fs::path pth(p.path, fs::native);
	it = fs::directory_iterator(pth);
	}catch ( const std::exception & ex ){
		cout<<"iterator exception"<<endl;
		return OK;
	}
	int count = 0;
	initOutput(tmpf, p, count);
	int added = 0;
	cout<<"p.path = "<<p.path<<endl;
	while(it != end){
		if(addFile(tmpf, p, it->string())){
			//done with the output
			doneOutput();
			sendOutput();
			++count;
			added = 0;
			initOutput(tmpf, p, count);
		}else{
			++added;
		}
		++it; 
	}
	cout<<"done"<<endl;
	tmpf.flush();
	tmpf.close();
	if(added){
		doneOutput();
		sendOutput();
	}
	return 0;
}

bool parseArguments(Params &_par, int argc, char *argv[]){
	try{
		TCLAP::CmdLine cmd("imapappender application", ' ', "0.1");
		
		TCLAP::ValueArg<std::string> host("a","addr","IMAP server host",false,"localhost","string");
		TCLAP::ValueArg<std::string> port("p","port","IMAP server port",false,"143","string");
		TCLAP::ValueArg<std::string> path("l","localpath","Path to local folder containing files",true,"","string");
		TCLAP::ValueArg<std::string> folder("f","imapfolder","imap folder to push data on",true,"","string");
		TCLAP::ValueArg<uint32> maxsz("m","maxsz","per mail maximum size",false,1024 * 1024 * 5,"uint32");
		TCLAP::SwitchArg ssl("s","ssl", "Use ssl for communication", false, false);
		
		cmd.add(host);
		cmd.add(port);
		cmd.add(path);
		cmd.add(folder);
		cmd.add(ssl);
		cmd.add(maxsz);
		
		// Parse the argv array.
		cmd.parse( argc, argv );
		
		_par.host = host.getValue();
		_par.port = port.getValue();
		_par.path = path.getValue();
		_par.folder = folder.getValue();
		_par.secure = ssl.getValue();
		_par.maxsz = maxsz.getValue();
		return false;
	}catch(TCLAP::ArgException &e){// catch any exceptions
		std::cerr << "error: " << e.error() << " for arg " << e.argId() << std::endl;
		return true;
	}
}

int initOutput(ofstream &_os, const Params &_p, int _cnt){
	//write the header:
	_os.close();
	_os.clear();
	_os.open("tmp.eml");
	cout<<"is open tem = "<<_os.is_open()<<endl;
	_os<<"Message-ID: <494B91F6.3050006@localhost>\r\n";
	_os<<"Date: Fri, 19 Dec 2008 14:22:14 +0200\r\n";
	_os<<"From: Valentin Palade <vpalade@localhost>\r\n";
	_os<<"X-Mozilla-Draft-Info: internal/draft; vcard=0; receipt=0; uuencode=0\r\n";
	_os<<"User-Agent: Thunderbird 2.0.0.14 (X11/20080501)\r\n";
	_os<<"MIME-Version: 1.0\r\n";
	_os<<"To: vipalade@yahoo.com\r\n";
	_os<<"Subject: pictures\r\n";
	_os<<"Content-Type: multipart/mixed;\r\n";
 	_os<<"boundary=\"------------090007060609050203080902\"\r\n";
	_os<<"\r\n";
	_os<<"This is a multi-part message in MIME format.\r\n";
	_os<<"--------------090007060609050203080902\r\n";
	_os<<"Content-Type: text/plain; charset=ISO-8859-1\r\n";
	_os<<"Content-Transfer-Encoding: 7bit\r\n";

	_os<<"some text\r\n";
	_os<<"--------------090007060609050203080902\r\n";
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

bool addFile(ofstream &_os, const Params &_p, const string &_s){
	ifstream ifs;
	ifs.open(_s.c_str());
	const char *name = strrchr(_s.c_str(), '/');
	if(!name) name = _s.c_str();
	else ++name;
	string ctp;
	contentType(ctp, _s);
	_os<<"Content-Type: "<<ctp<<";\r\n";
	_os<<"name=\""<<name<<"\"\r\n";
	_os<<"Content-Transfer-Encoding: base64\r\n";
	_os<<"Content-Disposition: inline;\r\n";
 	_os<<"filename=\""<<name<<"\"\r\n\r\n";
	
	cxxtools::Base64ostream b64os(_os);
	copystream(b64os, ifs);
	b64os.end();
	_os<<"\r\n";
	_os<<"--------------090007060609050203080902\r\n";
	return false;
}
int sendOutput(){
	ifstream ifs;
	ifs.open("tmp.eml");
	copystream(cout, ifs);
}

int doneOutput(){
	//_is<<"\r\n";
}

