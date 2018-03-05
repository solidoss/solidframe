// solid/utility/innerlist.hpp
//
// Copyright (c) 2007, 2008 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#pragma once

#include "solid/utility/common.hpp"

namespace solid {

namespace inner {
struct Link {
    size_t prev_;
    size_t next_;

    void clear()
    {
        prev_ = next_ = InvalidIndex();
    }

    Link(
        const size_t _prev = InvalidIndex(),
        const size_t _next = InvalidIndex())
        : prev_(_prev)
        , next_(_next)
    {
    }
};

template <size_t Size>
struct Node;

template <size_t Size>
Link& link_accessor(Node<Size>& _node, const size_t _index);

template <size_t Size>
Link const& link_const_accessor(Node<Size> const& _node, const size_t _index);

template <size_t Size>
struct Node {
    enum {
        NodeSize = Size,
    };

private:
    template <size_t Sz>
    friend Link& link_accessor(Node<Sz>& _node, const size_t _index);
    template <size_t Sz>
    friend Link const& link_const_accessor(Node<Sz> const& _node, const size_t _index);

    Link links[NodeSize];
};

template <size_t Size>
Link& link_accessor(Node<Size>& _node, const size_t _index)
{
    return _node.links[_index];
}

template <size_t Size>
Link const& link_const_accessor(Node<Size> const& _node, const size_t _index)
{
    return _node.links[_index];
}

template <class Vec, size_t LinkId>
class List {
public:
    typedef typename Vec::value_type ValueT;

    List(Vec& _rvec)
        : rvec_(_rvec)
        , size_(0)
        , back_(InvalidIndex())
        , front_(InvalidIndex())
    {
    }

    List(List<Vec, LinkId>&)  = delete;
    List(List<Vec, LinkId>&&) = delete;
    List(
        Vec&               _rvec,
        List<Vec, LinkId>& _rinnerlist)
        : rvec_(_rvec)
        , size_(_rinnerlist.size_)
        , back_(_rinnerlist.back_)
        , front_(_rinnerlist.front_)
    {
    }

    void pushBack(const size_t _index)
    {
        Link& rcrt_link = link(_index);

        rcrt_link = Link(back_, InvalidIndex());

        if (back_ != InvalidIndex()) {
            link(back_).next_ = _index;
            back_             = _index;
        } else {
            back_  = _index;
            front_ = _index;
        }

        ++size_;
    }

    void pushFront(const size_t _index)
    {
        Link& rcrt_link = link(_index);

        rcrt_link = Link(InvalidIndex(), front_);

        if (front_ != InvalidIndex()) {
            link(front_).prev_ = _index;
            front_             = _index;
        } else {
            back_  = _index;
            front_ = _index;
        }

        ++size_;
    }

    //insert in front of the sentinel
    void insertFront(const size_t _sentinel, const size_t _index)
    {
        Link& rsent_link = link(_sentinel);
        Link& rcrt_link  = link(_index);

        rcrt_link        = Link(rsent_link.prev_, _sentinel);
        rsent_link.prev_ = _index;

        if (front_ == _sentinel) {
            front_ = _index;
        } else {
            Link& lnk = link(rcrt_link.prev_);
            lnk.next_ = _index;
        }
        ++size_;
    }

    ValueT& front()
    {
        return rvec_[front_];
    }

    ValueT const& front() const
    {
        return rvec_[front_];
    }

    size_t frontIndex() const
    {
        return front_;
    }

    ValueT& back()
    {
        return rvec_[back_];
    }

    ValueT const& back() const
    {
        return rvec_[back_];
    }

    size_t backIndex() const
    {
        return back_;
    }

    void erase(const size_t _index)
    {
        Link& rcrt_link = link(_index);

        if (rcrt_link.prev_ != InvalidIndex()) {
            link(rcrt_link.prev_).next_ = rcrt_link.next_;
        } else {
            //first in the list
            front_ = rcrt_link.next_;
        }

        if (rcrt_link.next_ != InvalidIndex()) {
            link(rcrt_link.next_).prev_ = rcrt_link.prev_;
        } else {
            //last in the list
            back_ = rcrt_link.prev_;
        }
        --size_;
        rcrt_link.clear();
    }

    size_t popFront()
    {
        size_t old_front = front_;
        erase(front_);
        return old_front;
    }

    size_t popBack()
    {
        size_t old_back = back_;
        erase(back_);
        return old_back;
    }

    size_t size() const
    {
        return size_;
    }

    bool empty() const
    {
        return size_ == 0;
    }

    template <class F>
    void forEach(F _f)
    {
        size_t it = front_;

        while (it != InvalidIndex()) {
            const size_t crtit = it;
            it                 = link(it).next_;

            _f(crtit, rvec_[crtit]);
        }
    }

    template <class F>
    void forEach(F _f) const
    {
        size_t it = front_;

        while (it != InvalidIndex()) {
            const size_t crtit = it;
            it                 = link(it).next_;

            _f(crtit, rvec_[crtit]);
        }
    }

    void fastClear()
    {
        size_ = 0;
        back_ = front_ = InvalidIndex();
    }

    void clear()
    {
        while (size()) {
            popFront();
        }
    }

    size_t previousIndex(const size_t _index) const
    {
        return link(_index).prev_;
    }

    size_t nextIndex(const size_t _index) const
    {
        return link(_index).next_;
    }

    bool check() const
    {
        if (size() && (back_ == InvalidIndex() || front_ == InvalidIndex())) {
            return false;
        }
        if (back_ == InvalidIndex() || front_ == InvalidIndex()) {
            return back_ == front_;
        }

        return true;
    }

    bool contains(const size_t _index) const
    {
        return link(_index).prev_ != InvalidIndex() || link(_index).next_ != InvalidIndex() || _index == back_;
    }

private:
    Link& link(const size_t _index)
    {
        //typedef Vec::value_type       NodeT;
        return link_accessor(rvec_[_index], LinkId);
    }
    Link const& link(const size_t _index) const
    {
        //typedef Vec::value_type       NodeT;
        return link_const_accessor(rvec_[_index], LinkId);
    }

private:
    Vec&   rvec_;
    size_t size_;
    size_t back_;
    size_t front_;
};

} //namespace inner

} //namespace solid
