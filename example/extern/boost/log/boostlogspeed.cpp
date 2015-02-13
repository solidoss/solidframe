#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sinks/basic_sink_backend.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>

#include <boost/core/null_deleter.hpp>

#include <boost/thread/thread.hpp>
#include "utility/common.hpp"
#include "system/debug.hpp"

using namespace std;
using namespace solid;

namespace logging = boost::log;
namespace src = boost::log::sources;
namespace sinks = boost::log::sinks;
namespace keywords = boost::log::keywords;


namespace sinks = boost::log::sinks;
namespace expr = boost::log::expressions;

int loopcount = 100 ;
int size = 1000 * 1000;
int thr = 4;

size_t	testmodule;

namespace {
    boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level> logger;
}

void producer(){
	std::vector<size_t> v1(size);
	std::vector<size_t> v2(size);
	for(int i = 0; i < loopcount; ++i){
		
		for(size_t j = 0; j < size; ++j){
			v2[j] =  solid::bit_revert(v1[j] + j);
			//BOOST_LOG_SEV(logger, boost::log::trivial::debug)<<" Some value = "<<v2[j];
			wdbgx(testmodule, " Some value = "<<v2[j]);
		}
		
	}
}

int main(int argc, char *argv[]){
	
	if(argc >= 2){
		loopcount = atoi(argv[1]);
	}
	if(argc >= 3){
		size = atoi(argv[2]);
	}
	if(argc >= 4){
		thr = atoi(argv[3]);
	}
	
	//fsink->locked_backend()->auto_flush(true);
    
    typedef sinks::synchronous_sink< sinks::text_ostream_backend > text_sink;
	boost::shared_ptr< text_sink > sink = boost::make_shared< text_sink >();

	// Add a stream to write log to
	sink->locked_backend()->add_stream(boost::shared_ptr< std::ostream >(&std::clog, boost::null_deleter()));
	
	sink->set_formatter
	(
		expr::format("%1%:[%2%] %3%")
			% expr::attr< boost::posix_time::ptime >("TimeStamp")
			% logging::trivial::severity
			//% expr::attr< boost::thread::id >("ThreadID")
			% expr::smessage
	);
	
	logging::core::get()->add_sink(sink);
    
    logging::core::get()->set_filter
    (
        logging::trivial::severity > logging::trivial::debug
    );
    logging::add_common_attributes();
	
	
	boost::thread_group producer_threads;
	
	testmodule = solid::Debug::the().registerModule("test");
	
#ifdef UDEBUG
	{
	string dbgout;
	Debug::the().levelMask("view");
	Debug::the().moduleMask("test");
	
	Debug::the().initStdErr(
		false,
		&dbgout
	);
	
	cout<<"Debug output: "<<dbgout<<endl;
	dbgout.clear();
	Debug::the().moduleNames(dbgout);
	cout<<"Debug modules: "<<dbgout<<endl;
	}
#endif
	 
	for (int i = 0; i != thr; ++i){
		producer_threads.create_thread(producer);
	}
	
	producer_threads.join_all();
	
	return 0;
}

