#include <cstdlib>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#ifdef SOLID_ON_LINUX
#include <mcheck.h>
#include <malloc.h>
#endif

#include <boost/asio/ssl.hpp>

using boost::asio::ip::tcp;
using namespace std;

typedef boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket;

class session
{
public:
    session(boost::asio::io_service& io_service, boost::asio::ssl::context& context)
        : socket_(io_service, context)
    {
    }

    ssl_socket::lowest_layer_type& socket(){
        return socket_.lowest_layer();
    }


    void start()
    {
        //socket_.lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true));
        boost::asio::socket_base::receive_buffer_size   recvoption(1024 * 64);
        boost::asio::socket_base::send_buffer_size      sendoption(1024 * 32);
        socket().set_option(sendoption);
        socket().set_option(recvoption);
        //cout<<"recv_buffer_size = "<<recvoption.value()<<endl;
        //cout<<"send_buffer_size = "<<sendoption.value()<<endl;
        
        socket_.async_handshake(
            boost::asio::ssl::stream_base::server,
            boost::bind(&session::handle_handshake, this, boost::asio::placeholders::error)
        );
    }

private:
    
    void handle_handshake(const boost::system::error_code& error)
    {
        if (!error){
#ifdef USE_BIND
            socket_.async_read_some(
                boost::asio::buffer(data_, max_length),
                boost::bind(&session::handle_read, this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred
                )
            );
#else       
            socket_.async_read_some(
                boost::asio::buffer(data_, max_length),
                [this](const boost::system::error_code& _error, size_t _sz){handle_read(_error, _sz);}
            );
#endif  
        }else{
            delete this;
        }
    }
    
    void handle_read(const boost::system::error_code& error, size_t bytes_transferred)
    {
        if (!error)
        {
#ifdef USE_BIND
            boost::asio::async_write(
                socket_,
                boost::asio::buffer(data_, bytes_transferred),
                boost::bind(
                    &session::handle_write,
                    this,
                    boost::asio::placeholders::error
                )
            );
#else
            boost::asio::async_write(
                socket_,
                boost::asio::buffer(data_, bytes_transferred),
                [this](const boost::system::error_code& _error, size_t /*_sz*/){handle_write(_error);}
            );
#endif
        }
        else
        {
            delete this;
        }
    }

    void handle_write(const boost::system::error_code& error)
    {
        if (!error)
        {
#ifdef USE_BIND
            socket_.async_read_some(
                boost::asio::buffer(data_, max_length),
                boost::bind(
                    &session::handle_read,
                    this,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred
                )
            );
#else
            socket_.async_read_some(
                boost::asio::buffer(data_, max_length),
                [this](const boost::system::error_code& _error, size_t _sz){handle_read(_error, _sz);}
            );
#endif
        }
        else
        {
            delete this;
        }
    }

    ssl_socket socket_;
    enum { max_length = 1024 * 2 };
    char data_[max_length];
};

class server
{
public:
    server(boost::asio::io_service& io_service, short port)
        : io_service_(io_service),
        acceptor_(io_service, tcp::endpoint(tcp::v4(), port)), context_(boost::asio::ssl::context::sslv23)
    {
        
        context_.set_options(
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::single_dh_use
        );
        //context_.set_password_callback(boost::bind(&server::get_password, this));
        context_.use_certificate_chain_file("echo-server-cert.pem");
        context_.use_private_key_file("echo-server-key.pem", boost::asio::ssl::context::pem);
        start_accept();
    }

private:
    void start_accept()
    {
        session* new_session = new session(io_service_, context_);
        acceptor_.async_accept(new_session->socket(),
            boost::bind(&server::handle_accept, this, new_session,
            boost::asio::placeholders::error));
    }

    void handle_accept(session* new_session,
        const boost::system::error_code& error)
    {
        if (!error)
        {
            new_session->start();
        }
        else
        {
            delete new_session;
        }

        start_accept();
    }

    boost::asio::io_service     &io_service_;
    tcp::acceptor               acceptor_;
    boost::asio::ssl::context   context_;
};

int main(int argc, char* argv[])
{
    //mtrace();
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: boost_echoserver <port>\n";
            return 1;
        }

        boost::asio::io_service io_service;

        using namespace std; // For atoi.
        server s(io_service, atoi(argv[1]));

        io_service.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
