#ifndef DISTRIBUTED_CONCEPT_CORE_SIGNALS_HPP
#define DISTRIBUTED_CONCEPT_CORE_SIGNALS_HPP

#include "foundation/signal.hpp"
#include <string>

struct ConceptSignal: Dynamic<ConceptSignal, foundation::Signal>{
	
};

struct InsertSignal: Dynamic<InsertSignal, ConceptSignal>{
	InsertSignal(const std::string&, uint32 _pos);
};

struct FetchSignal: Dynamic<FetchSignal, ConceptSignal>{
	FetchSignal(const std::string&);
};

struct EraseSignal: Dynamic<EraseSignal, ConceptSignal>{
	EraseSignal(const std::string&);
	EraseSignal();
};

#endif
