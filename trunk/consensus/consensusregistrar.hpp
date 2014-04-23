// consensus/consensusregistrar.hpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#ifndef SOLID_CONSENSUS_REGISTRAR_HPP
#define SOLID_CONSENSUS_REGISTRAR_HPP

#include "frame/common.hpp"

namespace solid{
namespace consensus{

class Registrar{
public:
	static Registrar& the();
	frame::IndexT  registerObject(const frame::ObjectUidT &_robjuid, const frame::IndexT &_ridx = INVALID_INDEX);
	void unregisterObject(frame::IndexT &_ridx);
	frame::ObjectUidT objectUid(const frame::IndexT &_ridx)const;
private:
	Registrar();
	~Registrar();
	Registrar(const Registrar&);
	Registrar& operator=(const Registrar&);
private:
	struct Data;
	Data &d;
};


}//namespace consensus
}//namespace solid

#endif
