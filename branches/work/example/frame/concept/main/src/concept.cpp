// concept.cpp
//
// Copyright (c) 2007, 2008, 2013 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <iostream>
#include <sstream>
#include <signal.h>
#include "system/debug.hpp"
#include "system/mutex.hpp"
#include "system/condition.hpp"
#include "system/thread.hpp"
#include "system/socketaddress.hpp"

#include "core/manager.hpp"
#include "alpha/alphaservice.hpp"
#include "proxy/proxyservice.hpp"
#include "gamma/gammaservice.hpp"
#include "audit/log/logmanager.hpp"
#include "audit/log/logconnectors.hpp"
#include "audit/log.hpp"
#include "utility/iostream.hpp"
#include "system/directory.hpp"

#include "frame/ipc/ipcservice.hpp"

#include "boost/program_options.hpp"


using namespace std;
using namespace solid;

/*
	The proof of concept application.
	It instantiate a manager, creates some services,
	registers some listeners talkers on those services
	and offers a small CLI.
*/
// prints the CLI help
void printHelp();
// inserts a new talker
int insertTalker(char *_pc, int _len, concept::Manager &_rtm);


struct DeviceInputOutputStream: InputOutputStream {
    DeviceInputOutputStream(int _d, int _pd):d(_d), pd(_pd) {}
    void close() {
        int tmp = d;
        d = -1;
        if(pd > 0) {
            ::close(tmp);
            ::close(pd);
        }
    }
    /*virtual*/ int read(char *_pb, uint32 _bl, uint32 _flags = 0) {
        int rv = ::read(d, _pb, _bl);
        return rv;
    }
    /*virtual*/ int write(const char *_pb, uint32 _bl, uint32 _flags = 0) {
        return ::write(d, _pb, _bl);
    }
    int64 seek(int64, SeekRef) {
        return -1;
    }
    int d;
    int pd;
};

int pairfd[2];


struct Params {
    typedef std::vector<std::string>		StringVectorT;
    typedef std::vector<SocketAddressInet4>	SocketAddressInet4VectorT;

    bool prepare();

    int								start_port;
    uint32							network_id;
    string							dbg_levels;
    string							dbg_modules;
    string							dbg_addr;
    string							dbg_port;
    bool							dbg_buffered;
    bool							dbg_console;
    bool							log;
    StringVectorT					ipcgwvec;
    SocketAddressInet4VectorT		ipcgwaddrvec;
};

bool parseArguments(Params &_par, int argc, char *argv[]);

bool insertListener(
    concept::Service &_rsvc,
    const char *_name,
    const char *_addr,
    int _port,
    bool _secure
);

int main(int argc, char* argv[]) {
    signal(SIGPIPE, SIG_IGN);

    Params p;
    if(parseArguments(p, argc, argv)) return 0;
    if(!p.prepare()) {
        cout<<"Error preparing the arguments"<<endl;
        return 0;
    }

    cout<<"Built on SolidFrame version "<<SF_MAJOR<<'.'<<SF_MINOR<<'.'<<SF_PATCH<<endl;

    //this must be called from the main thread
    //so that the main thread can also have specific data
    //like any other threads from solidground::system::thread
    Thread::init();

#ifdef UDEBUG
    {
        string dbgout;
        Debug::the().levelMask(p.dbg_levels.c_str());
        Debug::the().moduleMask(p.dbg_modules.c_str());
        if(p.dbg_addr.size() && p.dbg_port.size()) {
            Debug::the().initSocket(
                p.dbg_addr.c_str(),
                p.dbg_port.c_str(),
                p.dbg_buffered,
                &dbgout
            );
        } else if(p.dbg_console) {
            Debug::the().initStdErr(
                p.dbg_buffered,
                &dbgout
            );
        } else {
            ostringstream oss;
            if(*argv[0] == '.') {
                oss<<argv[0] + 2;
            } else {
                oss<<argv[0];
            }
            oss<<'_'<<p.start_port;

            Debug::the().initFile(
                oss.str().c_str(),
                p.dbg_buffered,
                3,
                1024 * 1024 * 64,
                &dbgout
            );
        }
        cout<<"Debug output: "<<dbgout<<endl;
        dbgout.clear();
        Debug::the().moduleNames(dbgout);
        cout<<"Debug modules: "<<dbgout<<endl;
    }
#endif

    pipe(pairfd);
    audit::LogManager lm;
    if(p.log) {
        lm.start();
        lm.insertChannel(new DeviceInputOutputStream(pairfd[0], pairfd[1]));
        lm.insertListener("localhost", "3333");
        Directory::create("log");
        lm.insertConnector(new audit::LogBasicConnector("log"));
        solid::Log::the().reinit(argv[0],  new DeviceInputOutputStream(pairfd[1],-1), "ALL");
    }
    int stime;
    long ltime;

    ltime = time(NULL); /* get current calendar time */
    stime = (unsigned int) ltime/2;
    srand(stime);

    idbg("Built on SolidFrame version "<<SF_MAJOR<<'.'<<SF_MINOR<<'.'<<SF_PATCH);

    idbg("sizeof ulong = "<<sizeof(long));
#ifdef _LP64
    idbg("64bit architecture");
#else
    idbg("32bit architecture");
#endif

    do {

        concept::Manager			m;
        concept::alpha::Service		alphasvc(m);
        concept::proxy::Service		proxysvc(m);
        concept::gamma::Service		gammasvc(m);

        m.start();

        m.registerService(alphasvc);
        m.registerService(proxysvc);
        m.registerService(gammasvc);

        if(true) {
            int port = p.start_port + 222;
            frame::ipc::Configuration	cfg;
            ResolveData					rd = synchronous_resolve("0.0.0.0", port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
            bool						rv;

            cfg.baseaddr = rd.begin();

            for(Params::SocketAddressInet4VectorT::iterator it(p.ipcgwaddrvec.begin()); it != p.ipcgwaddrvec.end(); ++it) {
                cfg.gatewayaddrvec.push_back(SocketAddressInet(*it));
            }

            rv = m.ipc().reconfigure(cfg);

            if(!rv) {
                cout<<"Error configuring ipcservice: "/*<<err.toString()*/<<endl;
                m.stop();
                break;
            }
        }

        if(true) { //creates and registers a new alpha service
            if(!insertListener(alphasvc, "alpha", "0.0.0.0", p.start_port + 114, false)) {
                m.stop();
                break;
            }
            if(!insertListener(alphasvc, "alpha", "0.0.0.0", p.start_port + 124, true)) {
                m.stop();
                break;
            }
        }

        if(true) { //creates and registers a new alpha service
            if(!insertListener(proxysvc, "proxy", "0.0.0.0", p.start_port + 214, false)) {
                m.stop();
                break;
            }
        }

        if(true) { //creates and registers a new alpha service
            if(!insertListener(gammasvc, "gamma", "0.0.0.0", p.start_port + 314, false)) {
                m.stop();
                break;
            }
        }

        char buf[2048];
        int rc = 0;
        // the small CLI loop
        while(true) {
            if(rc == -1) {
                cout<<"Error: Parsing command line"<<endl;
            }
            if(rc == 1) {
                cout<<"Error: executing command"<<endl;
            }
            rc = 0;
            cout<<'>';
            cin.getline(buf,2048);
            if(!strcasecmp(buf,"quit") || !strcasecmp(buf,"q")) {
                m.stop();
                cout<<"signalled to stop"<<endl;
                break;
            }
            if(!strncasecmp(buf,"help",4)) {
                printHelp();
                continue;
            }
            cout<<"Error parsing command line"<<endl;
        }
    } while(false);
    lm.stop();
    Thread::waitAll();
    return 0;
}

void printHelp() {
    cout<<"Server commands:"<<endl;
    cout<<"[quit]:\tStops the server and exits the application"<<endl;
    cout<<"[help]:\tPrint this text"<<endl;
}

bool insertListener(
    concept::Service &_rsvc,
    const char *_name,
    const char *_addr,
    int _port,
    bool _secure
) {
    ResolveData		rd = synchronous_resolve(_addr, _port, 0, SocketInfo::Inet4, SocketInfo::Stream);

    if(!_rsvc.insertListener(rd, _secure)) {
        cout<<"["<<_name<<"] Failed adding listener on port "<<_port<<endl;
        return false;
    } else {
        cout<<"["<<_name<<"] Added listener on port "<<_port<<" objid = "<<endl;
        return true;
    }
}

static frame::ObjectUidT crtcon(frame::invalid_uid());

bool parseArguments(Params &_par, int argc, char *argv[]) {
    using namespace boost::program_options;
    try {
        options_description desc("SolidFrame concept application");
        desc.add_options()
        ("help,h", "List program options")
        ("base-port,b", value<int>(&_par.start_port)->default_value(1000),"Base port")
        ("network-id,n", value<uint32>(&_par.network_id)->default_value(0), "Network id")
        ("debug-levels,L", value<string>(&_par.dbg_levels)->default_value("view"),"Debug logging levels")
        ("debug-modules,M", value<string>(&_par.dbg_modules),"Debug logging modules")
        ("debug-address,A", value<string>(&_par.dbg_addr), "Debug server address (e.g. on linux use: nc -l 2222)")
        ("debug-port,P", value<string>(&_par.dbg_port), "Debug server port (e.g. on linux use: nc -l 2222)")
        ("debug-console,C", value<bool>(&_par.dbg_console)->implicit_value(true)->default_value(false), "Debug console")
        ("debug-unbuffered,S", value<bool>(&_par.dbg_buffered)->implicit_value(true)->default_value(false), "Debug unbuffered")
        ("use-log,l", value<bool>(&_par.log)->implicit_value(true)->default_value(false), "Debug buffered")
        ("gateway,g", value<vector<string> >(&_par.ipcgwvec), "IPC gateways")
        /*		("verbose,v", po::value<int>()->implicit_value(1),
        				"enable verbosity (optionally specify level)")*/
        /*		("listen,l", po::value<int>(&portnum)->implicit_value(1001)
        				->default_value(0,"no"),
        				"listen on a port.")
        		("include-path,I", po::value< vector<string> >(),
        				"include path")
        		("input-file", po::value< vector<string> >(), "input file")*/
        ;
        variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);
        notify(vm);
        if (vm.count("help")) {
            cout << desc << "\n";
            return true;
        }
        return false;
    } catch(exception& e) {
        cout << e.what() << "\n";
        return true;
    }
}

bool Params::prepare() {
    const int default_gw_ipc_port = 3333;
    size_t pos;
    for(std::vector<std::string>::iterator it(ipcgwvec.begin()); it != ipcgwvec.end(); ++it) {
        pos = it->find(':');
        if(pos == std::string::npos) {
            ResolveData		rd = synchronous_resolve(it->c_str(), default_gw_ipc_port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
            if(!rd.empty()) {
                ipcgwaddrvec.push_back(SocketAddressInet4(rd.begin()));
            } else {
                cout<<"Invalid address: "<<*it<<endl;
                return false;
            }
        } else {
            (*it)[pos] = '\0';
            int port = atoi(it->c_str() + pos + 1);
            ResolveData		rd = synchronous_resolve(it->c_str(), port, 0, SocketInfo::Inet4, SocketInfo::Datagram);
            if(!rd.empty()) {
                ipcgwaddrvec.push_back(SocketAddressInet4(rd.begin()));
            } else {
                cout<<"Invalid address: "<<*it<<endl;
                return false;
            }
        }
        cout<<"added ipc gateway address "<<ipcgwaddrvec.back()<<endl;
    }
    return true;
}
