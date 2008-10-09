#include "audit/log.hpp"



int main(int argc, char *argv[]){
	Log::instance().reinit(argv[0], Log::AllLevels, "any");
	return 0;
}
