#include <cstdlib>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/xtime.hpp>

#ifdef _WIN32
#include "winsock2.h"
#endif

using boost::asio::ip::tcp;
using namespace std;

static tcp::resolver::iterator resolved_endpoint;
boost::xtime delay(int secs)
{
    //Create an xtime that is secs seconds from now
    boost::xtime ret;
    boost::xtime_get(&ret, boost::TIME_UTC);
    ret.sec += secs;
    return ret;
}

class session
{
	
public:
    session(boost::asio::io_service& io_service)
        : socket_1(io_service), socket_2(io_service), pending(0)
    {
    }
    ~session(){
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
        ++pending;
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
        --pending;
        if(!error)
        {
            boost::asio::socket_base::send_buffer_size option(0);
            socketTwo().set_option(option);
            pending+=2;
            //issue async read on both sockets:
            socket_1.async_read_some(
                boost::asio::buffer(data_1, max_length),
                boost::bind(
                    &session::handle_read_one,
                    this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred
                )
            );
            socket_2.async_read_some(
                boost::asio::buffer(data_2, max_length),
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
        --pending;
        if (!error)
        {
            ++pending;
            boost::asio::async_write(
                socket_2,
                boost::asio::buffer(data_1, bytes_transferred),
                boost::bind(
                    &session::handle_write_two,
                    this,
                    boost::asio::placeholders::error
                )
            );
        }
        else
        {
            cout<<"error r1 v = "<<error.value()<<" n = "<<error.message()<<endl;
        if(!pending){
            cout<<"Delete socket read_one"<<endl;
            delete this;
        }else{
            int err =0;
            int vallen = sizeof(int);
            socket_1.close();
            socket_2.close();
        }

        }
    }

    void handle_read_two(
        const boost::system::error_code& error,
        size_t bytes_transferred
    )
    {
        --pending;
        if (!error)
        {
            ++pending;
            boost::asio::async_write(
                socket_1,
                boost::asio::buffer(data_2, bytes_transferred),
                boost::bind(
                    &session::handle_write_one,
                    this,
                    boost::asio::placeholders::error
                )
            );
        }
        else
        {
        cout<<"error r2 r = "<<error.value()<<" n = "<<error.message()<<endl;
        if(!pending){
            cout<<"Delete socket read_two"<<endl;
            delete this;
        }else{
            int err =0;
            int vallen = sizeof(int);
            
            socket_1.close();
            socket_2.close();
        }

        }
    }
    
    void handle_write_one(const boost::system::error_code& error)
    {
        --pending;
        if (!error)
        {
            ++pending;
            socket_2.async_read_some(
                boost::asio::buffer(data_2, max_length),
                boost::bind(
                    &session::handle_read_two, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred
                )
            );
        }
        else
        {
            cout<<"error w1 v = "<<error.value()<<" n = "<<error.message()<<endl;
        if(!pending){
            cout<<"Delete socket write_one"<<endl;
            delete this;
        }else{
            int err =0;
            int vallen = sizeof(int);
           
            socket_1.close();
            socket_2.close();
        }
            
        }
    }

    void handle_write_two(const boost::system::error_code& error)
    {
        --pending;
        if (!error)
        {
            ++pending;
            socket_1.async_read_some(
                boost::asio::buffer(data_1, max_length),
                boost::bind(
                    &session::handle_read_one,
                    this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred
                )
            );
        }
        else
        {
            cout<<"error w2 v = "<<error.value()<<" n = "<<error.message()<<endl;
        if(!pending){
            cout<<"Delete socket write_two"<<endl;
            delete this;
        }else{

            int err =0;
            int vallen = sizeof(int);
            socket_1.close();
            socket_2.close();
        }
        
        }
    }
private:
    tcp::socket socket_1;
    tcp::socket socket_2;
    enum { max_length = 2 * 1024 };
    char data_1[max_length];
    char data_2[max_length];
    unsigned long pending;
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
