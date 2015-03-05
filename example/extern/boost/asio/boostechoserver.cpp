#include <cstdlib>
#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <mcheck.h>
#include <malloc.h>

using boost::asio::ip::tcp;
using namespace std;
#if 0
static void *(*old_malloc_hook)(size_t, const void *);

static void *new_malloc_hook(size_t size, const void *caller) {
    void *mem;

    __malloc_hook = old_malloc_hook;
    mem = malloc(size);
    fprintf(stderr, "%p: malloc(%zu) = %p\n", caller, size, mem);
    __malloc_hook = new_malloc_hook;

    return mem;
}

static void init_my_hooks(void) {
    old_malloc_hook = __malloc_hook;
    __malloc_hook = new_malloc_hook;
}

void (*__MALLOC_HOOK_VOLATILE __malloc_initialize_hook)(void) = init_my_hooks;

void* operator new(size_t sz) throw (std::bad_alloc)
{
    cerr << "allocating " << sz << " bytes\n";
    void* mem = malloc(sz);
    if (mem)
        return mem;
    else
        throw std::bad_alloc();
}


void operator delete(void* ptr) throw()
{
    cerr << "deallocating at " << ptr << endl;
    free(ptr);
}

#endif

class session
{
public:
	session(boost::asio::io_service& io_service)
		: socket_(io_service)
	{
	}

	tcp::socket& socket()
	{
		return socket_;
	}

	void start()
	{
		//socket_.lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true));
		boost::asio::socket_base::receive_buffer_size	recvoption(1024 * 64);
		boost::asio::socket_base::send_buffer_size		sendoption(1024 * 32);
		socket_.set_option(sendoption);
		socket_.set_option(recvoption);
		//cout<<"recv_buffer_size = "<<recvoption.value()<<endl;
		//cout<<"send_buffer_size = "<<sendoption.value()<<endl;
		socket_.async_read_some(boost::asio::buffer(data_, max_length),
			boost::bind(&session::handle_read, this,
			boost::asio::placeholders::error,
			boost::asio::placeholders::bytes_transferred));
	}

private:
	void handle_read(const boost::system::error_code& error,
		size_t bytes_transferred)
	{
		if (!error)
		{
			boost::asio::async_write(socket_,
				boost::asio::buffer(data_, bytes_transferred),
				boost::bind(&session::handle_write, this,
					boost::asio::placeholders::error));
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
			socket_.async_read_some(boost::asio::buffer(data_, max_length),
				boost::bind(&session::handle_read, this,
					boost::asio::placeholders::error,
					boost::asio::placeholders::bytes_transferred));
		}
		else
		{
			delete this;
		}
	}

	tcp::socket socket_;
	enum { max_length = 1024 * 2 };
	char data_[max_length];
};

class server
{
public:
	server(boost::asio::io_service& io_service, short port)
		: io_service_(io_service),
		acceptor_(io_service, tcp::endpoint(tcp::v4(), port))
	{
		start_accept();
	}

private:
	void start_accept()
	{
		session* new_session = new session(io_service_);
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

	boost::asio::io_service& io_service_;
	tcp::acceptor acceptor_;
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