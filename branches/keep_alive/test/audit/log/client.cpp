#include "audit/log.hpp"



int main(int argc, char *argv[]){
	Log::instance().reinit(argv[0], Log::AllLevels, "any");
	ilog(Log::any, 0, "some message");
	ilog(Log::any, 1, "some message 1");
	ilog(Log::any, 2, "some message 2");
	ilog(Log::any, 3, "some message 3");
	return 0;
}

