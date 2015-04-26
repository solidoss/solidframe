// serialization/src/typeidmap.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "serialization/typeidmap.hpp"

namespace solid{
namespace serialization{

class ErrorCategory: public ErrorCategoryT{
public:
	enum{
		NoTypeE = 1,
		NoCastE,
	};
private:
	const char*   name() const noexcept (true){
		return "solid::serialization::TypeIdMap Error";
	}
	
    std::string    message(int _ev) const{
		switch(_ev){
			case NoTypeE:
				return "Type not registered";
			case NoCastE:
				return "Cast not registered";
			default:
				return "Unknown";
		}
	}
};

const ErrorCategory		ec;

/*static*/ ErrorConditionT TypeIdMapBase::error_no_cast(){
	return ErrorConditionT(ErrorCategory::NoCastE, ec);
}
/*static*/ ErrorConditionT TypeIdMapBase::error_no_type(){
	return ErrorConditionT(ErrorCategory::NoTypeE, ec);
}

}//namespace serialization
}//namespace solid
