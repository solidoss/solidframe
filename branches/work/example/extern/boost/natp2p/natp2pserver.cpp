#include <cstdlib>
#include <iostream>
#include <sstream>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
using boost::asio::ip::udp;
using namespace std;

enum { max_length = 1024 };


struct NatP2PServer{
	NatP2PServer(
		boost::asio::io_service	&_io_service,
		int _port
	):io_service(_io_service), sock(_io_service, udp::endpoint(udp::v4(), _port)){}
	
	void run();
private:
	void parseRequest(const char *_data, unsigned _len);
	void initCommand(istringstream &_iss);
	void connectCommand(istringstream &_iss);
	void send(udp::endpoint &_endpoint, ostringstream &_ros);
private:
	boost::asio::io_service	&io_service;
	udp::socket				sock;
	udp::endpoint			sender_endpoint;
};

int main(int argc, char* argv[])
{
	try
	{
		if (argc != 2)
		{
		std::cerr << "Usage: example_natp2p_server <port>\n";
		return 1;
		}

		boost::asio::io_service	io_service;
		NatP2PServer			srvr(io_service, atoi(argv[1]));
		srvr.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}


void NatP2PServer::run(){
	char data[max_length];
	for (;;)
	{
		size_t length = sock.receive_from(
			boost::asio::buffer(data, max_length),
			sender_endpoint
		);
		//sock.send_to(boost::asio::buffer(data, length), sender_endpoint);
		parseRequest(data, length);
	}
}

void NatP2PServer::parseRequest(const char *_data, unsigned _len){
	string 			str(_data, _len);
	istringstream	iss;
	cout<<"parseRequest[";
	cout.write(_data, _len)<<']'<<endl;
	iss.str(str);
	char			cmd;
	iss>>cmd;
	switch(cmd){
		case 'i':
		case 'I':
			initCommand(iss);
			break;
		case 'c':
		case 'C':
			connectCommand(iss);
			break;
		default:
			cout<<"skip command: "<<cmd<<" from "<<sender_endpoint<<endl;
			break;
	}
}

void NatP2PServer::initCommand(istringstream &_iss){
	cout<<"initCommand"<<endl;
	ostringstream oss;
	oss<<'i'<<' '<<sender_endpoint.address()<<' '<<sender_endpoint.port()<<endl;
	send(sender_endpoint, oss);
}
void NatP2PServer::connectCommand(istringstream &_iss){
	string	addr;
	int		port;
	
	_iss>>addr>>port;
	
	cout<<"connectCommand("<<addr<<','<<port<<')'<<endl;
	udp::endpoint endpoint;
	endpoint.address(boost::asio::ip::address::from_string(addr.c_str()));
	endpoint.port(port);
	
	ostringstream oss;
	
	oss<<'C'<<' '<<sender_endpoint.address()<<' '<<sender_endpoint.port()<<endl;
	
	send(endpoint, oss);
}

void NatP2PServer::send(udp::endpoint &_endpoint, ostringstream &_ros){
	string str = _ros.str();
	sock.send_to(boost::asio::buffer(str.c_str(), str.size()), _endpoint);
}
