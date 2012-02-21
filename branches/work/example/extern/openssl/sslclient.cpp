#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "system/cassert.hpp"
#include "system/thread.hpp"
#include "system/debug.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <poll.h>

#include <termios.h>
#include <unistd.h>

using namespace std;

#define CHK_SSL(err) if ((err)==-1) { ERR_print_errors_fp(stderr); exit(2); }


int tocrlf(char *_pb, const char *_bend);

int main(int argc, char *argv[]){
	
	if(argc != 3){
		cout<<"usage:\n$ echo_client addr port"<<endl;
		return 0;
	}
#ifdef UDEBUG
	{
	string s;
	Dbg::instance().levelMask("iew");
	Dbg::instance().moduleMask();
	Dbg::instance().initStdErr(false, &s);
	cout<<"Debug output: "<<s<<endl;
	s.clear();
	Dbg::instance().moduleBits(s);
	cout<<"Debug bits: "<<s<<endl;
	}
#endif

	termios tio;
	if(!tcgetattr(fileno(stdin), &tio)){
		tio.c_oflag |= ONLCR;
		if(tcsetattr(fileno(stdin), TCSANOW, &tio)){
			cout<<"failed tcsetattr "<<strerror(errno)<<endl;
		}
	}else{
		cout<<"failed tcgetattr "<<strerror(errno)<<endl;
	}
	//Initializing OpenSSL
	SSL_library_init();
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();
	
	//create a connection
	
	SocketAddressInfo ai(argv[1], argv[2], 0, SocketAddressInfo::Inet4, SocketAddressInfo::Stream, 0);
	
	if(ai.empty()){
		cout<<"no such address"<<endl;
		return 0;
	}
	SocketDevice sd;
	sd.create(ai.begin());
	
	pollfd pfds[2];
	
	pfds[0].fd = fileno(stdin);
	pfds[0].events = POLLIN;
	pfds[0].revents = 0;
	
	pfds[1].fd = sd.descriptor();
	pfds[1].events = POLLIN;
	pfds[1].revents = 0;
	
	SSL_CTX * ctx = SSL_CTX_new(SSLv23_client_method());
	SSL * ssl;
	
	if(!ctx){
		cout<<"failed SSL_CTX_new: "<<ERR_error_string(ERR_get_error(), NULL)<<endl;
		return 0;
	}
	const char *pcertpath = OSSL_SOURCE_PATH"ssl_/certs/A-client.pem";
	cout<<"Client certificate path: "<<pcertpath<<endl;
	if(! SSL_CTX_load_verify_locations(ctx, pcertpath, NULL)){
    	cout<<"failed SSL_CTX_load_verify_locations 1 "<<ERR_error_string(ERR_get_error(), NULL)<<endl;;
    	return 0;
	}
	system("mkdir certs");
	
	if(! SSL_CTX_load_verify_locations(ctx, NULL, "certs")){
		cout<<"failed SSL_CTX_load_verify_locations 2 "<<ERR_error_string(ERR_get_error(), NULL)<<endl;;
		return 0;
	}
	int err;
	ssl = SSL_new(ctx);

	if(!ssl){
		cout<<"failed SSL_new "<<ERR_error_string(ERR_get_error(), NULL)<<endl;;
		return 0;
	}
	cout<<"before connect"<<endl;
	if(sd.connect(ai.begin()) == BAD){
		cout<<"could not connect = "<<strerror(errno)<<endl;
		return 0;
	}
	
	sd.makeNonBlocking();
	err = SSL_set_fd(ssl, sd.descriptor());
	cout<<"done ssl_set_fd "<<err<<endl;
	ERR_print_errors_fp(stderr);
	//CHK_SSL(err);

	//SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
	while(true){
		err = SSL_connect(ssl);
		if(err < 0){
			if(SSL_want(ssl) != SSL_NOTHING){
				cout<<"want something"<<endl;
				if(SSL_want_read(ssl)){
					cout<<"want read"<<endl;
				}
				if(SSL_want_write(ssl)){
					cout<<"want write"<<endl;
				}
				Thread::sleep(1000);
			}else{
				cout<<"error ssl_connect"<<endl;
				return 10;
			}
		}else if(err == 0)
			return 11;
		else break;
		
	}
	ERR_print_errors_fp(stderr);
	cout<<"done SSL_connect"<<err<<endl;
	CHK_SSL(err);
	cout<<"done ssl connect"<<endl;
	
	cout<<"done ssl handshaking"<<endl;
	char	bufs[2][1024 + 128];
	char	*wbuf[2];
	int		readsz[2];
	int		states[2];
	readsz[0] = 0;
	bool read_socket = true, read_stdin = false;
	bool write_socket = false;
	bool wait_read_on_write = false;
	bool wait_write_on_read = false;
	bool wait_write_on_write = false;
	while(true){
		if(pfds[0].revents & POLLIN && !readsz[0]){
			read_stdin = true;
		}
		
		if(pfds[1].revents & POLLOUT){
			write_socket = write_socket || (readsz[0] != 0);
			read_socket = read_socket || wait_write_on_read;
		}
		if(pfds[1].revents & POLLIN){
			write_socket = write_socket || wait_read_on_write;
			read_socket = read_socket || (!wait_read_on_write);
			//TODO: maybe you only need:
			//read_socket = true;
		}
		
		if(pfds[1].revents && !(pfds[1].revents & (POLLIN | POLLOUT))){
			break;
		}
		
		if(read_stdin){//we can safely read from stdin
			readsz[0] = ::read(fileno(stdin), bufs[0], 1024);
			readsz[0] = tocrlf(bufs[0], bufs[0] + readsz[0]);
			wbuf[0] = bufs[0];
			pfds[0].events = 0;//dont wait for stdin
			write_socket = true;
			read_stdin = false;
			if(memcmp(bufs[0], "quit", 4) == 0) break;
		}
		
		if(write_socket){//write to socket
			int rv = SSL_write(ssl, wbuf[0], readsz[0]);
			wait_write_on_write = false;
			wait_read_on_write = false;
			if(!wait_write_on_read){
				pfds[1].events &= ~POLLOUT;
			}
			if(rv == readsz[0]){
				readsz[0] = 0;
				write_socket = false;
				pfds[0].events = POLLIN;//dont wait for stdin
			}else if(SSL_want(ssl) != SSL_NOTHING){
				cassert(pfds[1].events);
				write_socket = false;
				if(SSL_want_read(ssl)){
					pfds[1].events |= POLLIN;
					wait_read_on_write = true;
				}
				if(SSL_want_write(ssl)){
					pfds[1].events |= POLLOUT;
					wait_write_on_write = true;
				}
			}else if(rv > 0){
				wbuf[0] += rv;
				readsz[0] -= rv;
				//write_socket = true;
			}else{
				break;
			}
		}
		
		if(read_socket){
			//we can read:
			int rv = SSL_read(ssl, bufs[1], 8);
			wait_write_on_read = false;
			if(!wait_write_on_write){
				pfds[1].events &= ~POLLOUT;
			}
			if(rv > 0){
				cout.write(bufs[1], rv).flush();
				read_socket = true;
			}else if(rv < 0 && SSL_want(ssl) != SSL_NOTHING){
				if(SSL_want_read(ssl)){
					pfds[1].events |= POLLIN;
				}
				if(SSL_want_write(ssl)){
					pfds[1].events |= POLLOUT;
					wait_write_on_read = true;
				}
				read_socket = false;
			}else{
				cout<<"[ ... connection closed by peer ...]"<<endl;
				break;
			}
		}
		int pollcnt = poll(pfds, 2, (read_socket || read_stdin) ? 0 : -1);
		if(pollcnt <= 0) continue;
	}
	
	SSL_free(ssl);
	cout<<"Closing"<<endl;
	cout<<sd.write("closing", 4)<<endl;
	return 0;
}


int tocrlf(char *_pb, const char *_bend){
	if(_pb == _bend) return 0;
	int len = _bend - _pb;
	char b[2048];
	const char *ip = _pb;
	char *op = b;
	char last = 's';
	while(ip != _bend){
		if(*ip == '\n' and last != '\r'){
			*op = '\r';
			++op;
			*op = '\n';
			++op;
			++len;
			last = '\n';
		}else{
			*op = *ip;
			++op;
			last = *ip;
		}
		++ip;
	}
	memcpy(_pb, b, len);
	return len;
}
