#include "example/distributed/concept/core/signals.hpp"
#include "example/distributed/concept/core/manager.hpp"

foundation::Manager& m(){
	return foundation::m();
}

const foundation::IndexT& serverIndex(){
	static const foundation::IndexT idx(10);
	return idx;
}

InsertSignal::InsertSignal(const std::string&, uint32 _pos){
	
}

FetchSignal::FetchSignal(const std::string&){
	
}

EraseSignal::EraseSignal(const std::string&){
	
}

EraseSignal::EraseSignal(){
	
}
