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
		InitWait,
		Connect,
		ConnectWait,
		ConnectAcceptWait,
		RunStart,
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
		connect_endpoint(_connect_endpoint),state(Init){}
	
	
	void run();
	void runSend();
private:
	void parseRequest(const char *_data, unsigned _len);
	void initCommand(istringstream &_iss);
	void connectCommand(istringstream &_iss);
	void acceptCommand(istringstream &_iss);
	void send(udp::endpoint &_endpoint, ostringstream &_ros);
	void sendInit();
	void sendConnect();
	void sendAccept();
	void startSendThread();
 
private:
	boost::asio::io_service	&io_service;
	udp::socket				sock;
	udp::endpoint			sender_endpoint;
	udp::endpoint			server_endpoint;
	udp::endpoint			connect_endpoint;
	udp::endpoint			exit_endpoint;
	boost::thread			thread;
	int						state;
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
		switch(state){
			case Init:
				sendInit();
				break;
			case InitWait:{
				size_t length = sock.receive_from(
					boost::asio::buffer(data, max_length),
					sender_endpoint
				);
				parseRequest(data, length);
			}break;
			case Connect:
				sendConnect();
				break;
			case ConnectWait:{
				size_t length = sock.receive_from(
					boost::asio::buffer(data, max_length),
					sender_endpoint
				);
				parseRequest(data, length);
			}break;
			case ConnectAcceptWait:{
				size_t length = sock.receive_from(
					boost::asio::buffer(data, max_length),
					sender_endpoint
				);
				parseRequest(data, length);
			}break;
			case RunStart:
				startSendThread();
				state = Run;
			case Run:{
				size_t length = sock.receive_from(
					boost::asio::buffer(data, max_length),
					sender_endpoint
				);
				cout<<"Received "<<length<<" bytes from ["<<sender_endpoint<<"] ";
				cout.write(data, length);
				break;
			}
		}
	}
}

void NatP2PClient::runSend(){
	string s;
	while(true){
		cin>>s;
		sock.send_to(boost::asio::buffer(s.data(), s.size()), connect_endpoint);
	}
}
void NatP2PClient::sendInit(){
	ostringstream oss;
	oss<<'i'<<endl;
	send(server_endpoint, oss);
	state = InitWait;
}
void NatP2PClient::sendConnect(){
	static const boost::asio::ip::address	noaddr;
	if(connect_endpoint.address() == noaddr){
		state = ConnectWait;
		return;
	}
	{	
		ostringstream oss;
		oss<<'c'<<' '<<connect_endpoint.address()<<' '<<connect_endpoint.port()<<endl;
		send(server_endpoint, oss);
	}
	{
		ostringstream oss;
		oss<<'c'<<' '<<exit_endpoint.address()<<' '<<exit_endpoint.port()<<endl;
		send(connect_endpoint, oss);
	}
	state = ConnectAcceptWait;
}
void NatP2PClient::sendAccept(){
	ostringstream oss;
	oss<<'a'<<endl;
	send(connect_endpoint, oss);
	state = RunStart;
}

struct Runner{
	NatP2PClient	&client;
	Runner(NatP2PClient &_client):client(_client){
	}
	void operator()(){
		client.runSend();
	}
};

void NatP2PClient::startSendThread(){
	Runner			r(*this);
	//boost::thread	thrd(sr);
	this->thread = boost::thread(r);
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
		case 'a':
		case 'A':
			acceptCommand(iss);
			break;
		default:
			cout<<"skip command: "<<cmd<<" from "<<sender_endpoint<<endl;
			break;
	}
}
void NatP2PClient::initCommand(istringstream &_iss){
	string	addr;
	int 	port;
	_iss>>addr>>port;
	cout<<"initCommand("<<addr<<','<<port<<')'<<endl;
	state = Connect;
	exit_endpoint.address(boost::asio::ip::address::from_string(addr.c_str()));
	exit_endpoint.port(port);
}
void NatP2PClient::connectCommand(istringstream &_iss){
	string	addr;
	int		port;
	
	_iss>>addr>>port;
	
	cout<<"connectCommand("<<addr<<','<<port<<')'<<endl;
	udp::endpoint endpoint;
	endpoint.address(boost::asio::ip::address::from_string(addr.c_str()));
	endpoint.port(port);
	connect_endpoint = endpoint;
	
	sendAccept();
}
void NatP2PClient::acceptCommand(istringstream &_iss){
	//connect_endpoint = sender_endpoint;
	cout<<"acceptCommand connect_endpoint = "<<connect_endpoint<<" sender_endpoint = "<<sender_endpoint<<endl;
	state = RunStart;
}
void NatP2PClient::send(udp::endpoint &_endpoint, ostringstream &_ros){
	string str = _ros.str();
	sock.send_to(boost::asio::buffer(str.c_str(), str.size()), sender_endpoint);
}

