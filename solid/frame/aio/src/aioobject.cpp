// solid/frame/aio/src/aioobject.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com) 
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include "solid/system/cassert.hpp"

#include "solid/frame/aio/aioobject.hpp"
#include "solid/frame/aio/aioreactor.hpp"
#include "solid/frame/aio/aiocompletion.hpp"
#include "solid/frame/aio/aioreactorcontext.hpp"

#include "solid/frame/service.hpp"

namespace solid{
namespace frame{
namespace aio{
//---------------------------------------------------------------------
//----  Object  ----
//---------------------------------------------------------------------

Object::Object(){}

/*virtual*/ void Object::onEvent(ReactorContext &_rctx, Event &&_uevent){
    
}

bool Object::isRunning()const{
    return runId().isValid();
}

bool Object::registerCompletionHandler(CompletionHandler &_rch){
    _rch.pnext = this->pnext;
    if(_rch.pnext){
        _rch.pnext->pprev = &_rch;
    }
    this->pnext = &_rch;
    _rch.pprev = this;
    return isRunning();
}

void Object::registerCompletionHandlers(){
    CompletionHandler *pch = this->pnext;
    
    while(pch != nullptr){
        pch->activate(*this);
        pch = pch->pnext;
    }
}

bool Object::doPrepareStop(ReactorContext &_rctx){
    if(this->disableVisits(_rctx.service().manager())){
        CompletionHandler *pch = this->pnext;
        
        while(pch != nullptr){
            pch->pprev = nullptr;//unregister
            pch->deactivate();
            
            pch = pch->pnext;
        }
        this->pnext = nullptr;
        return true;
    }
    return false;
}

}//namespace aio
}//namespace frame
}//namespace solid

