#include "solid/system/cassert.hpp"
#include "solid/utility/innerlist.hpp"
#include <iostream>
#include <vector>

using namespace std;

using namespace solid;

enum {
    InnerListOrder = 0,
    InnerListPending,
    InnerListSending,
    InnerListCache,

    InnerListCount //do not change
};

enum {
    InnerLinkOrder = 0,
    InnerLinkStatus,

    InnerLinkCount
};

struct Node : InnerNode<InnerLinkCount> {
    Node(const std::string& _rstr)
        : str(_rstr)
    {
    }
    std::string str;
};

typedef std::vector<Node> NodeVectorT;

typedef InnerList<NodeVectorT, InnerLinkOrder> InnerOrderListT;

int test_innerlist(int argc, char* argv[])
{
    NodeVectorT     node_vec;
    InnerOrderListT order_list(node_vec);

    SOLID_ASSERT(order_list.empty());

    node_vec.push_back(Node("a"));
    node_vec.push_back(Node("b"));
    node_vec.push_back(Node("c"));
    node_vec.push_back(Node("d"));
    node_vec.push_back(Node("e"));

    order_list.pushBack(4);
    SOLID_ASSERT(order_list.size() == 1);
    order_list.pushBack(3);
    SOLID_ASSERT(order_list.size() == 2);
    order_list.pushBack(2);
    SOLID_ASSERT(order_list.size() == 3);
    order_list.pushBack(1);
    SOLID_ASSERT(order_list.size() == 4);
    order_list.pushBack(0);
    SOLID_ASSERT(order_list.size() == 5);

    cout << "order_list(" << order_list.size() << "): ";

    auto visit_fnc = [](const size_t _index, Node const& _rnode) { cout << _rnode.str << ' '; };

    order_list.forEach(visit_fnc);
    cout << endl;

    cout << "front = " << order_list.front().str << endl;

    order_list.erase(3);
    SOLID_ASSERT(order_list.size() == 4);

    cout << "after erase(3) order_list(" << order_list.size() << "): ";
    order_list.forEach(visit_fnc);
    cout << endl;

    order_list.popFront();
    SOLID_ASSERT(order_list.size() == 3);

    cout << "after popFront order_list(" << order_list.size() << "): ";
    order_list.forEach(visit_fnc);
    cout << endl;

    return 0;
}