#include "openssl/bio.h"
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "system/socketaddress.hpp"
#include "system/socketdevice.hpp"
#include "system/cassert.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <poll.h>

#include <termios.h>
#include <unistd.h>

using namespace std;

int tocrlf(char *_pb, const char *_bend);

int main(int argc, char *argv[]){
	
	if(argc != 3){
		cout<<"usage:\n$ echo_client addr port"<<endl;
		return 0;
	}
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
	SSL_load_error_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_algorithms();
	
	//create a connection
	
	AddrInfo ai(argv[1], argv[2]);
	
	if(ai.empty()){
		cout<<"no such address"<<endl;
		return 0;
	}
	SocketDevice sd;
	sd.create(ai.begin());
	if(sd.connect(ai.begin()) == BAD){
		cout<<"could not connect = "<<strerror(errno)<<endl;
		return 0;
	}
	sd.makeNonBlocking();
	pollfd pfds[2];
	
	pfds[0].fd = fileno(stdin);
	pfds[0].events = POLLIN;
	pfds[0].revents = 0;
	
	pfds[1].fd = sd.descriptor();
	pfds[1].events = POLLIN;
	pfds[1].revents = 0;
	
	BIO *bio = BIO_new_socket(sd.descriptor(), 0);
	
	char	bufs[2][1024 + 128];
	char	*wbuf[2];
	int		readsz[2];
	int		states[2];
	//SSL_get_error
	//SSL_ERROR_WANT_READ
	//Edge triggered 
	readsz[0] = 0;
	bool read_socket = true, read_stdin = false;
	bool write_socket = false;
	bool wait_read_on_write = false;
	bool wait_write_on_read = false;
	bool wait_write_on_write = false;
	while(true){
/*		int pollcnt = poll(pfds, 2, -1);
		if(pollcnt <= 0) continue;*/
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
		
		if(read_stdin){//we can safely read from stdin
			readsz[0] = ::read(fileno(stdin), bufs[0], 1024);
			readsz[0] = tocrlf(bufs[0], bufs[0] + readsz[0]);
			wbuf[0] = bufs[0];
			pfds[0].events = 0;//dont wait for stdin
			write_socket = true;
			read_stdin = false;
		}
		
		if(write_socket){//write to socket
			int rv = BIO_write(bio, wbuf[0], readsz[0]);
			wait_write_on_write = false;
			wait_read_on_write = false;
			if(!wait_write_on_read){
				pfds[1].events &= ~POLLOUT;
			}
			if(rv == readsz[0]){
				readsz[0] = 0;
				write_socket = false;
				pfds[0].events = POLLIN;//dont wait for stdin
			}else if(BIO_should_retry(bio)){
				cassert(pfds[1].events);
				write_socket = false;
				if(BIO_should_read(bio)){
					pfds[1].events |= POLLIN;
					wait_read_on_write = true;
				}
				if(BIO_should_write(bio)){
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
			int rv = BIO_read(bio, bufs[1], 8);
			wait_write_on_read = false;
			if(!wait_write_on_write){
				pfds[1].events &= ~POLLOUT;
			}
			if(rv > 0){
				cout.write(bufs[1], rv).flush();
				read_socket = true;
			}else if(BIO_should_retry(bio)){
				if(BIO_should_read(bio)){
					pfds[1].events |= POLLIN;
				}
				if(BIO_should_write(bio)){
					pfds[1].events |= POLLOUT;
					wait_write_on_read = true;
				}
				read_socket = false;
			}else{
				break;
			}
		}
		int pollcnt = poll(pfds, 2, (read_socket || read_stdin) ? 0 : -1);
		if(pollcnt <= 0) continue;
	}
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

/*
while(true){
		int pollcnt = poll(pfds, 2, -1);
		if(pollcnt <= 0) continue;
		if(pfds[0].revents){//we can read from std in
			readsz[0] = ::read(fileno(stdin), bufs[0], 1024);
			wbuf[0] = bufs[0];
			//write it on socket:
			//int rv = sd.write(wbuf[0], readsz[0]);
			int rv == readsz[0] = BIO_write(bio, wbuf[0], readsz[0]);
			//if(rv < 0 && sd.shouldWait() || rv > 0 && rv < readsz[0]){
			if(rv <= 0 && BIO_should_retry(bio)){
				if(rv < 0) rv = 0;
				readsz[0] -= rv;
				wbuf[0] += rv;
				pfds[1].events |= POLLOUT;//wait for ready to write
				pfds[0].events = 0;//dont wait for input
			}else if(rv > 0){//all was written
				//do nothing
				readsz[0] = 0;
			}else{
				break;
			}
		}
		if(pfds[1].revents & POLLIN){//we can read from socket:
			readsz[1] = sd.read(bufs[1], 1024);
			if(readsz[1] < 0 && sd.shouldWait()){
				//do nothing
			}else if(readsz[1] > 0){//we've read something
				cout.write(bufs[1], readsz[1]).flush();
			}else{
				break;
			}
		}
		if(pfds[1].revents & POLLOUT){//we can write to socket:
			int rv = sd.write(wbuf[0], readsz[0]);
			if(rv < 0 && sd.shouldWait() || rv > 0 && rv < readsz[0]){
				if(rv < 0) rv = 0;
				readsz[0] -= rv;
				wbuf[0] += rv;
			}else if(rv > 0){//all was written
				readsz[0] = 0;
				wbuf[0] = NULL;
				pfds[0].events |= POLLIN;//wait for ready to write
			}else{
				break;
			}
		}
	}
*/
