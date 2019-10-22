// solid/utility/dynamic.hpp
//
// Copyright (c) 2010 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include <vector>

#include "solid/system/cassert.hpp"
#include "solid/system/common.hpp"

#include "solid/utility/common.hpp"
#include "solid/utility/dynamicpointer.hpp"
#include <atomic>

namespace solid {

//----------------------------------------------------------------
//      DynamicBase
//----------------------------------------------------------------
typedef std::vector<size_t> DynamicIdVectorT;

//struct DynamicPointerBase;
//! A base for all types that needs dynamic typeid.
struct DynamicBase {
    static bool isType(const size_t /*_id*/)
    {
        return false;
    }
    //! Get the type id for a Dynamic object.
    virtual size_t dynamicTypeId() const = 0;

    static bool isTypeDynamic(const size_t _id);

    static void staticTypeIds(DynamicIdVectorT& /*_rv*/)
    {
    }
    virtual void dynamicTypeIds(DynamicIdVectorT& /*_rv*/) const
    {
    }

protected:
    static size_t generateId();
    DynamicBase()
        : usecount(0)
    {
    }

    friend class DynamicPointerBase;
    virtual ~DynamicBase();

    //! Used by DynamicPointer - smartpointers
    size_t use();
    //! Used by DynamicPointer to know if the object must be deleted
    /*!
     * For the return value, think of use count. Returning zero means the object should
     * be deleted. Returning non zero means the object should not be deleted.
     */
    size_t release();

private:
    typedef std::atomic<size_t> AtomicSizeT;

    AtomicSizeT usecount;
};

//----------------------------------------------------------------
//      Dynamic
//----------------------------------------------------------------

//! Template class to provide dynamic type functionality
/*!
    If you need to have dynamic type functionality for your objects,
    you need to inherit them through Dynamic. Here's an example:<br>
    if you need to have:<br>
        C: public B <br>
        B: public A <br>
    you will actually have:<br>
        C: public Dynamic\<C,B><br>
        B: public Dynamic\<B,A><br>
        A: public Dynamic\<A>
*/
template <class X, class T = DynamicBase>
struct Dynamic : T {
    typedef Dynamic<X, T> BaseT;

    template <typename... Args>
    explicit Dynamic(Args&&... _args)
        : T(std::forward<Args>(_args)...)
    {
    }

    //!The static type id
    static size_t staticTypeId()
    {
        static const size_t id(DynamicBase::generateId());
        return id;
    }
    //TODO: add:
    //static bool isTypeExplicit(const DynamicBase*);
    static bool isType(const size_t _id)
    {
        if (_id == staticTypeId())
            return true;
        return T::isType(_id);
    }
    //! The dynamic typeid
    virtual size_t dynamicTypeId() const
    {
        return staticTypeId();
    }
    static bool isTypeDynamic(const size_t _id)
    {
        if (_id == staticTypeId())
            return true;
        return T::isTypeDynamic(_id);
    }

    static X* cast(DynamicBase* _pdb)
    {
        if (_pdb && isTypeDynamic(_pdb->dynamicTypeId())) {
            return static_cast<X*>(_pdb);
        }
        return nullptr;
    }

    static const X* cast(const DynamicBase* _pdb)
    {
        if (isTypeDynamic(_pdb->dynamicTypeId())) {
            return static_cast<const X*>(_pdb);
        }
        return nullptr;
    }

    static void staticTypeIds(DynamicIdVectorT& _rv)
    {
        _rv.push_back(BaseT::staticTypeId());
        T::staticTypeIds(_rv);
    }

    /*virtual*/ void dynamicTypeIds(DynamicIdVectorT& _rv) const
    {
        staticTypeIds(_rv);
    }

    DynamicPointer<X> dynamicFromThis() const
    {
        return DynamicPointer<X>(this);
    }
};

} //namespace solid
