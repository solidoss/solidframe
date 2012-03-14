#include <cstdlib>
#include <iostream>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
using boost::asio::ip::udp;
using namespace std;

enum { max_length = 1024 };

struct NatP2PClient{
	enum States{
		Init,
		Connect,
		Run
	};
	NatP2PClient(
		boost::asio::io_service	&_io_service,
		int _port,
		const udp::endpoint &_server_endpoint,
		const udp::endpoint &_connect_endpoint
	):	io_service(_io_service),
		sock(_io_service, udp::endpoint(udp::v4(), _port)),
		server_endpoint(_server_endpoint),
		connect_endpoint(_connect_endpoint){}
	
	
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
	udp::endpoint			server_endpoint;
	udp::endpoint			connect_endpoint;
};


int main(int argc, char* argv[])
{
	try
	{
		if (argc != 2)
		{
		std::cerr << "Usage: example_natp2p_client port server_addr server_port [connect_addr connect_port]\n";
		return 1;
		}

		boost::asio::io_service io_service;
		udp::endpoint			server_endpoint;
		udp::endpoint			connect_endpoint;
		
		server_endpoint.address(boost::asio::ip::address::from_string(argv[2]));
		server_endpoint.port(atoi(argv[3]));
		
		if(argc == 6){
			connect_endpoint.address(boost::asio::ip::address::from_string(argv[4]));
			connect_endpoint.port(atoi(argv[5]));
		}
		
		NatP2PClient			client(
			io_service,
			atoi(argv[1]),
			server_endpoint,
			connect_endpoint
		);
		client.run();
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}

void NatP2PClient::run(){
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
void NatP2PClient::parseRequest(const char *_data, unsigned _len){
	string 			str(_data, _len);
	istringstream	iss;
	iss.str(str);
	char			cmd;
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
void NatP2PClient::initCommand(istringstream &_iss){
	cout<<"initCommand"<<endl;
	ostringstream oss;
	oss<<'i'<<' '<<sender_endpoint.address()<<' '<<sender_endpoint.port()<<endl;
	send(sender_endpoint, oss);
}
void NatP2PClient::connectCommand(istringstream &_iss){
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

void NatP2PClient::send(udp::endpoint &_endpoint, ostringstream &_ros){
	string str = _ros.str();
	sock.send_to(boost::asio::buffer(str.c_str(), str.size()), sender_endpoint);
}

