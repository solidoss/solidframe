	#include <cstdlib>
	#include <iostream>
	#include <boost/bind.hpp>
	#include <boost/asio.hpp>
	#include <boost/thread/thread.hpp>
	#include <boost/thread/xtime.hpp>
	#include "boost/shared_ptr.hpp"

	#ifdef _WIN32
	#include "winsock2.h"
	#endif

	using boost::asio::ip::tcp;
	using namespace std;

	static tcp::resolver::iterator resolved_endpoint;

	class session
	{
		
	public:
		session(boost::asio::io_service& io_service)
			: socket_1(io_service), socket_2(io_service)
		{
			rpos1 = wpos1 = wend1 = bbeg1;
			rpos2 = wpos2 = wend2 = bbeg2;
			pending_read1 = pending_read2 = false;
			pending_write1 = pending_write2 = 0;
		}
		~session(){
		}
		
		bool pending()const{
			return pending_read1 || pending_read2 || pending_write1 != 0 || pending_write2 != 0;
		}

		tcp::socket& socketOne()
		{
			return socket_1;
		}
		tcp::socket& socketTwo()
		{
			return socket_2;
		}

		void start()
		{
			//open a new socket and issue connect
			//socket_1 is accepted ok, connecting socket 2
			boost::system::error_code   err;
			socket_2.open(tcp::v4(), err);
			//very important - need to make the socket non blocking
			tcp::socket::non_blocking_io non_blocking_io(true);
			socketTwo().io_control(non_blocking_io);
			pending_read2 = true;
			socket_2.async_connect(
				*resolved_endpoint,
				boost::bind(
					&session::handle_connect,
					this,
					boost::asio::placeholders::error
				)
			);
		}
		
		void handle_connect(const boost::system::error_code& error)
		{
			pending_read2 = false;
			if(!error)
			{
				//boost::asio::socket_base::send_buffer_size option(0);
				//socketTwo().set_option(option);
				pending_read1 = true;
				//issue async read on both sockets:
				socket_1.async_read_some(
					boost::asio::buffer(bbeg1, max_length),
					boost::bind(
						&session::handle_read_one,
						this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);
				pending_read2 = true;
				socket_2.async_read_some(
					boost::asio::buffer(bbeg2, max_length),
					boost::bind(
						&session::handle_read_two,
						this,
						boost::asio::placeholders::error,
						boost::asio::placeholders::bytes_transferred
					)
				);
			}
			else
			{
				cout<<"Delete socket 1"<<endl;
				delete this;
			}
		}
		
		void handle_read_one(
			const boost::system::error_code& error,
			size_t bytes_transferred
		)
		{
			pending_read1 = false;
			if (!error)
			{
				rpos1 += bytes_transferred;
				
				size_t rcap;
				if(wpos1 <= rpos1){
					rcap = (bbeg1 + max_length) - rpos1;
					wend1 = rpos1;
					if(rcap < read_min_capacity){
						rpos1 = bbeg1;
						rcap = wpos1 - rpos1;
					}
				}else{
					rcap = wpos1 - rpos1;
				}
					
				if(rcap >= read_min_capacity){
					pending_read1 = true;
					socket_1.async_read_some(
						boost::asio::buffer(rpos1, rcap),
						boost::bind(
							&session::handle_read_one,
							this,
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred
						)
					);
				}
				if(!pending_write2){
					pending_write2 = wend1 - wpos1;
				
					if(pending_write2){
						socket_2.async_write_some(
							boost::asio::buffer(wpos1, pending_write2),
							boost::bind(
								&session::handle_write_two,
								this,
								boost::asio::placeholders::error,
								boost::asio::placeholders::bytes_transferred
							)
						);
					}
				}
			}
			else
			{
				cout<<"error r1 v = "<<error.value()<<" n = "<<error.message()<<endl;
				if(!pending()){
					cout<<"Delete socket read_one"<<endl;
					delete this;
				}else{
					socket_1.close();
					socket_2.close();
				}

			}
		}

		void handle_read_two(
			const boost::system::error_code& error,
			size_t bytes_transferred
		){
			pending_read2 = false;
			if (!error)
			{
				rpos2 += bytes_transferred;
				
				size_t rcap;
				if(wpos2 <= rpos2){
					rcap = (bbeg2 + max_length) - rpos2;
					wend2 = rpos2;
					if(rcap < read_min_capacity){
						rpos2 = bbeg2;
						rcap = wpos2 - rpos2;
					}
				}else{
					rcap = wpos2 - rpos2;
				}
					
				if(rcap >= read_min_capacity){
					pending_read2 = true;
					socket_2.async_read_some(
						boost::asio::buffer(rpos2, rcap),
						boost::bind(
							&session::handle_read_two,
							this,
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred
						)
					);
				}
				
				if(!pending_write1){
					pending_write1 = wend2 - wpos2;
					
					if(pending_write1){
						socket_1.async_write_some(
							boost::asio::buffer(wpos2, pending_write1),
							boost::bind(
								&session::handle_write_one,
								this,
								boost::asio::placeholders::error,
								boost::asio::placeholders::bytes_transferred
							)
						);
					}
				}
			}
			else
			{
				cout<<"error r1 v = "<<error.value()<<" n = "<<error.message()<<endl;
				if(!pending()){
					cout<<"Delete socket read_one"<<endl;
					delete this;
				}else{
					socket_1.close();
					socket_2.close();
				}

			}
		}
		
		void handle_write_one(
			const boost::system::error_code& error,
			size_t bytes_transferred
		){
			pending_write1 = 0;
			if (!error)
			{
				wpos2 += bytes_transferred;
				
				
				if(wpos2 == wend2 && rpos2 < wend2){
					wpos2 = bbeg2 + max_length;
					wend2 = wpos2;
				}
				
				if(!pending_read2){
					//see if we've freed enought space to do a read
					assert(wpos2 > rpos2);
					size_t rcap = wpos2 - rpos2;
					if(rcap >= read_min_capacity){
						pending_read2 = true;
						socket_2.async_read_some(
							boost::asio::buffer(rpos2, rcap),
							boost::bind(
								&session::handle_read_two,
								this,
								boost::asio::placeholders::error,
								boost::asio::placeholders::bytes_transferred
							)
						);
					}
				}
				
				if(wpos2 == (bbeg2 + max_length)){
					wpos2 = bbeg2;
					wend2 = rpos2;
				}
				
				pending_write1 = wend2 - wpos2;
				
				if(pending_write1){
					socket_1.async_write_some(
						boost::asio::buffer(wpos2, pending_write1),
						boost::bind(
							&session::handle_write_one,
							this,
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred
						)
					);
				}
			}
			else
			{
				cout<<"error w2 v = "<<error.value()<<" n = "<<error.message()<<endl;
				if(!pending()){
					cout<<"Delete socket write_two"<<endl;
					delete this;
				}else{
					socket_1.close();
					socket_2.close();
				}
			
			}
		}

		void handle_write_two(
			const boost::system::error_code& error,
			size_t bytes_transferred
		){
			pending_write2 = 0;
			if (!error)
			{
				wpos1 += bytes_transferred;
				
				
				if(wpos1 == wend1 && rpos1 < wend1){
					wpos1 = bbeg1 + max_length;
					wend1 = wpos1;
				}
				
				if(!pending_read1){
					//see if we've freed enought space to do a read
					assert(wpos1 > rpos1);
					size_t rcap = wpos1 - rpos1;
					if(rcap >= read_min_capacity){
						pending_read1 = true;
						socket_1.async_read_some(
							boost::asio::buffer(rpos1, rcap),
							boost::bind(
								&session::handle_read_one,
								this,
								boost::asio::placeholders::error,
								boost::asio::placeholders::bytes_transferred
							)
						);
					}
				}
				
				if(wpos1 == (bbeg1 + max_length)){
					wpos1 = bbeg1;
					wend1 = rpos1;
				}
				
				pending_write2 = wend1 - wpos1;
				
				if(pending_write2){
					socket_2.async_write_some(
						boost::asio::buffer(wpos1, pending_write2),
						boost::bind(
							&session::handle_write_two,
							this,
							boost::asio::placeholders::error,
							boost::asio::placeholders::bytes_transferred
						)
					);
				}
			}
			else
			{
				cout<<"error w2 v = "<<error.value()<<" n = "<<error.message()<<endl;
				if(!pending()){
					cout<<"Delete socket write_two"<<endl;
					delete this;
				}else{
					socket_1.close();
					socket_2.close();
				}
			
			}
		}
	private:
		enum {
			max_length = 16 * 1024,
			read_min_capacity = 1024
		};
		
		tcp::socket		socket_1;
		tcp::socket		socket_2;
		bool			pending_read1;
		uint16_t		pending_write1;
		bool			pending_read2;
		uint16_t		pending_write2;
		char			bbeg1[max_length];
		char			bbeg2[max_length];
		char			*rpos1;
		char			*wpos1;
		char			*rpos2;
		char			*wpos2;
		char			*wend1;
		char			*wend2;
	};

	class server
	{
	public:
		server(boost::asio::io_service& io_service, short port)
			: io_service_(io_service),
			acceptor_(io_service/*, tcp::endpoint(tcp::v4(), port)*/)
			
		{
			boost::asio::ip::address_v4	localaddr;
			localaddr.from_string("127.0.0.1");
			boost::asio::ip::tcp::endpoint endpoint(localaddr, port);
			acceptor_.open(endpoint.protocol());
			acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
			acceptor_.bind(endpoint);
			acceptor_.listen();
			
			session* new_session = new session(io_service_);
			acceptor_.async_accept(new_session->socketOne(),
				boost::bind(&server::handle_accept, this, new_session,
				boost::asio::placeholders::error));
		}

		void handle_accept(session* new_session,
			const boost::system::error_code& error)
		{
			if (!error)
			{
			// boost::asio::socket_base::send_buffer_size option(0);
			//new_session->socketOne().set_option(option);
			//very important - need to make the socket non blocking
			tcp::socket::non_blocking_io non_blocking_io(true);
			new_session->socketOne().io_control(non_blocking_io);
			new_session->start();
			new_session = new session(io_service_);
			acceptor_.async_accept(new_session->socketOne(),
				boost::bind(&server::handle_accept, this, new_session,
					boost::asio::placeholders::error));
			}
			else
			{
				delete new_session;
			}
		}

	private:
		boost::asio::io_service& io_service_;
		tcp::acceptor acceptor_;
	};

	struct ServiceRunner{
		ServiceRunner(boost::asio::io_service &_ris):ris(_ris){}
		void operator()(){
			ris.run();
		}
		boost::asio::io_service &ris;
	};

	struct MessagePoster{
		MessagePoster(const std::string &_msg):msg(_msg){}
		void operator()(){
			using namespace std;
			cout<<"executing message: "<<msg<<endl;
		}
		std::string msg;
	};

	int main(int argc, char* argv[])
	{
		try
		{
			//shared_ptr<string>			strptr;
			boost::shared_ptr<string>	bstrptr;
			//cout<<"sizeof(shared_ptr<string> = "<<sizeof(strptr)<<endl;
			cout<<"sizeof(boost::shared_ptr<string> = "<<sizeof(bstrptr)<<endl;
			if (argc != 4)
			{
			std::cerr << "Usage: proxyserver listen_port proxy_host proxy_port\n";
			return 1;
			}

			boost::asio::io_service io_service;
			
			using namespace std; // For atoi.
			server s(io_service, atoi(argv[1]));
			ServiceRunner sr(io_service);
			boost::thread thrd(sr);

			tcp::resolver resolver(io_service);
			tcp::resolver::query query(argv[2], argv[3]);
			resolved_endpoint = resolver.resolve(query);
			tcp::resolver::iterator end;
			if(resolved_endpoint == end)
			{
				cout<<"no such address "<<endl;
			}
			boost::asio::io_service::work work(io_service);
			string c;
			while(true){
				cin>>c;
				if(c == "q" || c == "Q" || c == "quit") break;
				//default messages are sent to print:
				io_service.post(MessagePoster(c));
			}
			io_service.stop();
			thrd.join();
		}
		catch (std::exception& e)
		{
			std::cerr << "Exception: " << e.what() << "\n";
		}

		return 0;
	}
