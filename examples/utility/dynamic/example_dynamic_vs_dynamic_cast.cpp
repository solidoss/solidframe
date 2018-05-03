#include "solid/system/cassert.hpp"
#include "solid/system/log.hpp"
#include "solid/utility/dynamicpointer.hpp"
#include "solid/utility/dynamictype.hpp"
#include "solid/utility/queue.hpp"
#include <iostream>
#include <queue>
#include <vector>

using namespace solid;
using namespace std;

using SizeTVectorT = std::vector<size_t>;

class B : public Dynamic<B> {
public:
    virtual std::ostream& print(std::ostream& _ros) const
    {
        _ros << "B";
        return _ros;
    }

    virtual void store(SizeTVectorT& _rv) const
    {
    }
};

template <size_t V1>
class A : public Dynamic<A<V1>, B> {
    /*virtual*/ std::ostream& print(std::ostream& _ros) const
    {
        _ros << "A<" << V1 << ">";
        return _ros;
    }

protected:
    /*virtual*/ void store(SizeTVectorT& _rv) const
    {
        _rv.push_back(V1);
    }
};

template <size_t V2, size_t V1>
class AA : public Dynamic<AA<V2, V1>, A<V1>> {
    /*virtual*/ std::ostream& print(std::ostream& _ros) const
    {
        _ros << "AA<" << V2 << ',' << V1 << ">";
        return _ros;
    }

protected:
    /*virtual*/ void store(SizeTVectorT& _rv) const
    {
        A<V1>::store(_rv);
        _rv.push_back(V2);
    }
};

template <size_t V3, size_t V2, size_t V1>
class AAA : public Dynamic<AAA<V3, V2, V1>, AA<V2, V1>> {
    /*virtual*/ std::ostream& print(std::ostream& _ros) const
    {
        _ros << "AAA<" << V3 << ',' << V2 << ',' << V1 << ">";
        return _ros;
    }

protected:
    /*virtual*/ void store(SizeTVectorT& _rv) const
    {
        AA<V2, V1>::store(_rv);
        _rv.push_back(V3);
    }
};

template <size_t V4, size_t V3, size_t V2, size_t V1>
class AAAA : public Dynamic<AAAA<V4, V3, V2, V1>, AAA<V3, V2, V1>> {
    /*virtual*/ std::ostream& print(std::ostream& _ros) const
    {
        _ros << "AAAA<" << V4 << ',' << V3 << ',' << V2 << ',' << V1 << ">";
        return _ros;
    }

protected:
    /*virtual*/ void store(SizeTVectorT& _rv) const
    {
        AAA<V3, V2, V1>::store(_rv);
        _rv.push_back(V4);
    }
};

using BDynamicPointerT = DynamicPointer<B>;
//using DynamicQueueT = std::queue<BDynamicPointerT>;
using DynamicQueueT = Queue<BDynamicPointerT>;

const size_t loop_count  = 100;
const size_t queue_count = 10000;

size_t dynamic_main(const size_t _loop_count, const size_t _queue_count)
{
    DynamicQueueT dq;
    size_t        cnt = 0;
    for (size_t j = 0; j < _loop_count; ++j) {

        for (size_t i = 0; i < _queue_count; ++i) {
            dq.push(BDynamicPointerT(new A<1>));
            dq.push(BDynamicPointerT(new AA<1, 2>));
            dq.push(BDynamicPointerT(new AAA<1, 2, 3>));
            dq.push(BDynamicPointerT(new AAAA<1, 2, 3, 4>));
        }

        SizeTVectorT sv;

        while (dq.size()) {
            B& rb = *dq.front();

            //rb.print(cout)<<" = ";

            if (A<1>::cast(&rb)) {
                //cout<<"A<1>";
                ++cnt;
            }

            if (AA<1, 2>::cast(&rb)) {
                //cout<<"AA<1,2>";
                ++cnt;
            }

            if (AAA<1, 2, 3>::cast(&rb)) {
                //cout<<"AAA<1,2,3>";
                ++cnt;
            }

            if (AAAA<1, 2, 3, 4>::cast(&rb)) {
                //cout<<"AAAA<1,2,3,4>";
                ++cnt;
            }
            //cout<<endl;

            if (A<2>::cast(&rb)) {
                SOLID_ASSERT(false);
            }

            if (AA<2, 2>::cast(&rb)) {
                SOLID_ASSERT(false);
            }

            if (AAA<2, 2, 2>::cast(&rb)) {
                SOLID_ASSERT(false);
            }

            if (AAAA<2, 2, 2, 2>::cast(&rb)) {
                SOLID_ASSERT(false);
            }

            dq.pop();
        }

    } //for j
    return cnt;
}

size_t dcast_main(const size_t _loop_count, const size_t _queue_count)
{
    DynamicQueueT dq;
    size_t        cnt = 0;
    for (size_t j = 0; j < _loop_count; ++j) {

        for (size_t i = 0; i < _queue_count; ++i) {
            dq.push(BDynamicPointerT(new A<1>));
            dq.push(BDynamicPointerT(new AA<1, 2>));
            dq.push(BDynamicPointerT(new AAA<1, 2, 3>));
            dq.push(BDynamicPointerT(new AAAA<1, 2, 3, 4>));
        }

        SizeTVectorT sv;

        while (dq.size()) {
            B& rb = *dq.front();

            //rb.print(cout)<<" = ";

            if (dynamic_cast<A<1>*>(&rb)) {
                //cout<<"A<1>";
                ++cnt;
            }

            if (dynamic_cast<AA<1, 2>*>(&rb)) {
                //cout<<"AA<1,2>";
                ++cnt;
            }

            if (dynamic_cast<AAA<1, 2, 3>*>(&rb)) {
                //cout<<"AAA<1,2,3>";
                ++cnt;
            }

            if (dynamic_cast<AAAA<1, 2, 3, 4>*>(&rb)) {
                //cout<<"AAAA<1,2,3,4>";
                ++cnt;
            }
//cout<<endl;
#if 0
            if(dynamic_cast<A<5>*>(&rb)){
                SOLID_ASSERT(false);
            }

            if(dynamic_cast<AA<5, 5>*>(&rb)){
                SOLID_ASSERT(false);
            }

            if(dynamic_cast<AAA<5, 5, 5>*>(&rb)){
                SOLID_ASSERT(false);
            }

            if(dynamic_cast<AAAA<5, 5, 5, 5>*>(&rb)){
                SOLID_ASSERT(false);
            }
#endif
            dq.pop();
        }

    } //for j
    return cnt;
}

int main(int argc, char* argv[])
{
    const char* choice_str = "0";

    if (argc >= 2) {
        choice_str = argv[1];
    }

    int choice = atoi(choice_str);

    switch (choice) {
    case 0:
        cout << "Dynamic_main: " << dynamic_main(loop_count, queue_count) << endl;
        break;
    case 1:
        cout << "Dcast_main: " << dcast_main(loop_count, queue_count) << endl;
        break;
    default:
        cout << "Unknown choice" << endl;
    }
}
