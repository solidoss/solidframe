// serialization_reinit.cpp
//
// Copyright (c) 2013 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//
#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <stack>
#include <vector>
//#undef SOLID_HAS_DEBUG
#include "solid/serialization/v1/binary.hpp"
#include "solid/system/debug.hpp"
#include "solid/system/socketaddress.hpp"
#include "solid/utility/dynamictype.hpp"
#include <vector>

using namespace std;
using namespace solid;

struct Test : Dynamic<Test> {
    virtual ~Test() {}
    virtual void print() const = 0;
};

struct TestA : Dynamic<TestA, Test> {
    TestA(int _a = 1, short _b = 2, unsigned _c = 3)
        : a(_a)
        , b(_b)
        , c(_c)
    {
    }
    template <class S>
    void solidSerializeV1(S& _s)
    {
        _s.push(a, "a::a").push(b, "a::b").push(c, "a::c");
    }
    int32_t  a;
    int16_t  b;
    uint32_t c;
    void     print() const { cout << "testa: a = " << a << " b = " << b << " c = " << c << endl; }
};

struct TestB : Dynamic<TestB, Test> {
    TestB(int _a = 4)
        : a(_a)
    {
    }
    int32_t a;
    void    print() const { cout << "testb: a = " << a << endl; }
    template <class S>
    void solidSerializeV1(S& _s)
    {
        _s.push(a, "b::a");
    }
};

class Container {
    typedef std::vector<Test*> TestVectorT;

public:
    Container()
        : crtidx(0)
    {
    }
    ~Container()
    {
        clear();
    }
    void clear()
    {
        for (TestVectorT::const_iterator it(tstvec.begin()); it != tstvec.end(); ++it) {
            delete *it;
        }
        tstvec.clear();
    }
    void print() const
    {
        for (TestVectorT::const_iterator it(tstvec.begin()); it != tstvec.end(); ++it) {
            (*it)->print();
        }
    }
    void push(Test* _ptst)
    {
        tstvec.push_back(_ptst);
    }
    template <class S>
    void solidSerializeV1(S& _s)
    {
        _s.template pushCall([this](S& _rs, uint64_t _val, ErrorConditionT& _rerr) { return serializationReinit(_rs, _val, _rerr); }, 0, "Test::call");
    }

    template <class S>
    void serializationReinit(S& _rs, const uint64_t& _rv, ErrorConditionT& _rerr)
    {
        if (S::IsSerializer) {
            idbg("ser 1");
            if (crtidx < tstvec.size()) {
                idbg("ser 2");
                Test* pmsg = tstvec[crtidx];
                ++crtidx;
                if (pmsg->dynamicTypeId() == TestA::staticTypeId()) {
                    crtsndmsg = TestA::staticTypeId();
                    _rs.push(*static_cast<TestA*>(pmsg), "message");
                    _rs.push(crtsndmsg, "typeid");
                } else if (pmsg->dynamicTypeId() == TestB::staticTypeId()) {
                    crtsndmsg = TestB::staticTypeId();
                    _rs.push(*static_cast<TestB*>(pmsg), "message");
                    _rs.push(crtsndmsg, "typeid");
                }

                return;
            } else if (crtidx == tstvec.size()) {
                idbg("ser 3");
                ++crtidx;
                crtsndmsg = 0xff;
                _rs.push(crtsndmsg, "typeid");
                return;
            } else {
                idbg("ser 4");
                return;
            }
        } else {
            if (_rv == 0) {
                idbg("des 1");
                _rs.template pushCall([this](S& _rs, uint64_t _val, ErrorConditionT& _rerr) { return serializationReinit(_rs, _val, _rerr); }, 1, "Test::call");
                _rs.push(crtrcvmsg, "typeid");
            } else {
                idbg("des 2");
                if (crtrcvmsg == TestA::staticTypeId()) {
                    TestA* pmsg = new TestA;
                    tstvec.push_back(pmsg);
                    _rs.push(*pmsg, "message");
                } else if (crtrcvmsg == TestB::staticTypeId()) {
                    TestB* pmsg = new TestB;
                    tstvec.push_back(pmsg);
                    _rs.push(*pmsg, "message");
                } else {
                    return;
                }
            }
            return;
        }
    }

private:
    TestVectorT tstvec;
    size_t      crtidx;
    uint8_t     crtsndmsg;
    uint8_t     crtrcvmsg;
};

typedef serialization::binary::Serializer<>   BinSerializerT;
typedef serialization::binary::Deserializer<> BinDeserializerT;

int main(int argc, char* argv[])
{
#ifdef SOLID_HAS_DEBUG
    Debug::the().levelMask();
    Debug::the().moduleMask();
    Debug::the().initStdErr(false);
#endif
    const int blen = 16;
    char      bufs[1000][blen];
    int       rv;

    {
        BinSerializerT ser;
        int            v = 0, cnt = 0;
        Container      c;

        c.push(new TestA);
        c.push(new TestB);

        ser.push(c, "container");

        while ((rv = ser.run(bufs[v], blen)) == blen) {
            cnt += rv;
            ++v;
        }
        if (rv < 0) {
            cout << "ERROR: serialization: " << ser.error().message() << endl;
            return 0;
        } else {
            cnt += rv;
        }
        cout << "Write size = " << cnt << endl;
    }
    cout << "Deserialization: =================================== " << endl;
    {
        BinDeserializerT des;
        int              v   = 0;
        int              cnt = 0;
        Container        c;

        des.push(c, "container");

        while ((rv = des.run(bufs[v], blen)) == blen) {
            cnt += rv;
            ++v;
        }

        if (rv < 0) {
            cout << "ERROR: deserialization " << des.error().message() << endl;
            return 0;
        }

        cnt += rv;
        cout << "Read size = " << cnt << endl;

        c.print();
    }
    return 0;
}
