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
		const udp::endpoint &_local_endpoint,
		const udp::endpoint &_server_endpoint,
		const udp::endpoint &_connect_nat_endpoint,
		const udp::endpoint &_connect_fwl_endpoint
	):	io_service(_io_service),
		sock(_io_service, udp::endpoint(udp::v4(), _local_endpoint.port())),
		local_endpoint(_local_endpoint),
		server_endpoint(_server_endpoint),
		connect_nat_endpoint(_connect_nat_endpoint),
		connect_fwl_endpoint(_connect_fwl_endpoint), state(Init){}
	
	void run();
	void runSend();
private:
	void parseRequest(const char *_data, unsigned _len);
	void initCommand(istringstream &_iss);
	void connectCommand(istringstream &_iss);
	void connectStunCommand(istringstream &_iss);
	void acceptCommand(istringstream &_iss);
	void send(udp::endpoint &_endpoint, ostringstream &_ros);
	void sendInit();
	void sendConnect(bool _send_to_server = true);
	void sendAccept();
	void startSendThread();
 
private:
	boost::asio::io_service	&io_service;
	udp::socket				sock;
	udp::endpoint			local_endpoint;
	udp::endpoint			sender_endpoint;
	udp::endpoint			server_endpoint;
	udp::endpoint			connect_nat_endpoint;
	udp::endpoint			connect_fwl_endpoint;
	udp::endpoint			peer_endpoint;
	udp::endpoint			exit_endpoint;
	boost::thread			thread;
	int						state;
};


int main(int argc, char* argv[])
{
// 	try
// 	{
		if (argc < 4)
		{
			std::cerr << "Usage: example_natp2p_client local_addr local_port server_addr server_port [connect_nat_addr connect_nat_port connect_fwl_addr connect_fwl_port]\n";
			return 1;
		}

		boost::asio::io_service io_service;
		udp::endpoint			local_endpoint;
		udp::endpoint			server_endpoint;
		udp::endpoint			connect_nat_endpoint;
		udp::endpoint			connect_fwl_endpoint;
		
		
		local_endpoint.address(boost::asio::ip::address::from_string(argv[1]));
		local_endpoint.port(atoi(argv[2]));
		
		server_endpoint.address(boost::asio::ip::address::from_string(argv[3]));
		server_endpoint.port(atoi(argv[4]));
		
		if(argc >= 7){
			connect_nat_endpoint.address(boost::asio::ip::address::from_string(argv[5]));
			connect_nat_endpoint.port(atoi(argv[6]));
		}
		
		if(argc >= 9){
			connect_fwl_endpoint.address(boost::asio::ip::address::from_string(argv[7]));
			connect_fwl_endpoint.port(atoi(argv[8]));
		}
		
		NatP2PClient			clnt(
			io_service,
			local_endpoint,
			server_endpoint,
			connect_nat_endpoint,
			connect_fwl_endpoint
		);
		
		clnt.run();
//	}
// 	catch (std::exception& e)
// 	{
// 		std::cerr << "Exception: " << e.what() << "\n";
// 	}

	return 0;
}

void NatP2PClient::run(){
	char data[max_length];
	cout<<"run"<<endl;
	for (;;)
	{
		switch(state){
			case Init:
				cout<<"state Init"<<endl;
				sendInit();
				break;
			case InitWait:{
				cout<<"state InitWait"<<endl;
				size_t length = sock.receive_from(
					boost::asio::buffer(data, max_length),
					sender_endpoint
				);
				cout<<"Received "<<length<<" bytes from ["<<sender_endpoint<<"] [";
				cout.write(data, length)<<']'<<endl;
				parseRequest(data, length);
			}break;
			case Connect:
				cout<<"state Connect"<<endl;
				sendConnect();
				break;
			case ConnectWait:{
				cout<<"state ConnectWait"<<endl;
				size_t length = sock.receive_from(
					boost::asio::buffer(data, max_length),
					sender_endpoint
				);
				cout<<"Received "<<length<<" bytes from ["<<sender_endpoint<<"] [";
				cout.write(data, length)<<']'<<endl;
				parseRequest(data, length);
			}break;
			case ConnectAcceptWait:{
				cout<<"state ConnectAcceptWait"<<endl;
				size_t length = sock.receive_from(
					boost::asio::buffer(data, max_length),
					sender_endpoint
				);
				cout<<"Received "<<length<<" bytes from ["<<sender_endpoint<<"] [";
				cout.write(data, length)<<']'<<endl;
				parseRequest(data, length);
			}break;
			case RunStart:
				cout<<"state RunStart"<<endl;
				startSendThread();
				state = Run;
			case Run:{
				cout<<"state Run"<<endl;
				size_t length = sock.receive_from(
					boost::asio::buffer(data, max_length),
					sender_endpoint
				);
				cout<<"Received "<<length<<" bytes from ["<<sender_endpoint<<"] [";
				cout.write(data, length)<<']'<<endl;
				break;
			}
		}
	}
}

void NatP2PClient::runSend(){
	string s;
	while(true){
		cin>>s;
		sock.send_to(boost::asio::buffer(s.data(), s.size()), peer_endpoint);
		cout<<"send data to "<<peer_endpoint<<": ["<<s<<']'<<endl;
	}
}
void NatP2PClient::sendInit(){
	cout<<"sendInit"<<endl;
	ostringstream oss;
	oss<<'i'<<endl;
	send(server_endpoint, oss);
	state = InitWait;
}
void NatP2PClient::sendConnect(bool _send_to_server){
	static const boost::asio::ip::address	noaddr;
	state = ConnectWait;
	if(connect_nat_endpoint.address() == noaddr){
		return;
	}
	if(_send_to_server){	
		ostringstream oss;
		oss<<'c'<<' '<<connect_nat_endpoint.address()<<' '<<connect_nat_endpoint.port()<<' '<<local_endpoint.address()<<' '<<local_endpoint.port()<<endl;
		send(server_endpoint, oss);
	}
	{
		ostringstream oss;
		oss<<'c'<<' '<<exit_endpoint.address()<<' '<<exit_endpoint.port()<<endl;
		send(connect_nat_endpoint, oss);
	}
	{	
		ostringstream oss;
		oss<<'c'<<' '<<local_endpoint.address()<<' '<<local_endpoint.port()<<endl;
		send(connect_fwl_endpoint, oss);
	}
}
void NatP2PClient::sendAccept(){
	ostringstream oss;
	oss<<'a'<<endl;
	send(peer_endpoint, oss);
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
	//boost::thread	thrd(r);
	this->thread = boost::thread(r);
}
void NatP2PClient::parseRequest(const char *_data, unsigned _len){
	string 			str(_data, _len);
	istringstream	iss;
	iss.str(str);
	char			cmd;
	iss>>cmd;
	switch(cmd){
		case 'i':
		case 'I':
			initCommand(iss);
			break;
		case 'c':
			connectCommand(iss);
			break;
		case 'C':
			connectStunCommand(iss);
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
	cout<<"initCommand("<<addr<<' '<<port<<')'<<endl;
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
	peer_endpoint = sender_endpoint;
	
	sendAccept();
}

void NatP2PClient::connectStunCommand(istringstream &_iss){
	string	nat_addr;
	int		nat_port;
	string	fwl_addr;
	int		fwl_port;
	
	_iss>>nat_addr>>nat_port;
	_iss>>fwl_addr>>fwl_port;
	
	cout<<"connectStunCommand("<<nat_addr<<','<<nat_port<<'-'<<fwl_addr<<','<<fwl_port<<')'<<endl;
	udp::endpoint nat_endpoint;
	udp::endpoint fwl_endpoint;
	
	nat_endpoint.address(boost::asio::ip::address::from_string(nat_addr.c_str()));
	nat_endpoint.port(nat_port);
	
	fwl_endpoint.address(boost::asio::ip::address::from_string(fwl_addr.c_str()));
	fwl_endpoint.port(fwl_port);
	
	connect_nat_endpoint = nat_endpoint;
	connect_fwl_endpoint = fwl_endpoint;
	
	sendConnect(false);
}

void NatP2PClient::acceptCommand(istringstream &_iss){
	peer_endpoint = sender_endpoint;
	cout<<"acceptCommand connect_endpoint = "<<peer_endpoint<<" sender_endpoint = "<<sender_endpoint<<endl;
	state = RunStart;
}
void NatP2PClient::send(udp::endpoint &_endpoint, ostringstream &_ros){
	string str = _ros.str();
	cout<<"send data to "<<_endpoint<<": ["<<str<<']'<<endl;
	sock.send_to(boost::asio::buffer(str.c_str(), str.size()), _endpoint);
}

