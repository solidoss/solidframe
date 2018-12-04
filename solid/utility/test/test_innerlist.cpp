#include "solid/system/cassert.hpp"
#include "solid/utility/innerlist.hpp"
#include <iostream>
#include <string>
#include <vector>

using namespace std;

using namespace solid;

enum {
    InnerLinkOrder = 0,
    InnerLinkStatus,

    InnerLinkCount
};

struct Node : inner::Node<InnerLinkCount> {
    Node(const std::string& _rstr, const size_t _idx)
        : str(_rstr)
        , idx(_idx)
    {
    }
    std::string str;
    size_t      idx;
};

typedef std::vector<Node> NodeVectorT;

typedef inner::List<NodeVectorT, InnerLinkOrder> InnerOrderListT;

int test_innerlist(int /*argc*/, char* /*argv*/ [])
{
    NodeVectorT     node_vec;
    InnerOrderListT order_list(node_vec);

    solid_assert(order_list.empty());

    node_vec.push_back(Node("a", 0));
    node_vec.push_back(Node("b", 1));
    node_vec.push_back(Node("c", 2));
    node_vec.push_back(Node("d", 3));
    node_vec.push_back(Node("e", 4));

    order_list.pushBack(0);
    solid_assert(order_list.size() == 1);
    order_list.pushBack(1);
    solid_assert(order_list.size() == 2);
    order_list.pushBack(2);
    solid_assert(order_list.size() == 3);
    order_list.pushBack(3);
    solid_assert(order_list.size() == 4);
    order_list.pushBack(4);
    solid_assert(order_list.size() == 5);

    cout << "order_list(" << order_list.size() << "): ";

    auto visit_fnc = [](const size_t _index, Node const& _rnode) { cout << _rnode.str << ' '; solid_assert(_index == _rnode.idx); };

    order_list.forEach(visit_fnc);
    cout << endl;

    cout << "front = " << order_list.front().str << endl;

    order_list.erase(3);
    solid_assert(order_list.size() == 4);

    cout << "after erase(3) order_list(" << order_list.size() << "): ";
    order_list.forEach(visit_fnc);
    cout << endl;

    order_list.popFront();
    solid_assert(order_list.size() == 3);

    cout << "after popFront order_list(" << order_list.size() << "): ";
    order_list.forEach(visit_fnc);
    cout << endl;

    order_list.popFront();
    solid_assert(order_list.size() == 2);

    solid_assert(order_list.front().idx == 2);
    solid_assert(order_list.back().idx == 4);

    order_list.popFront();

    solid_assert(order_list.size() == 1);

    solid_assert(order_list.frontIndex() == order_list.backIndex());

    order_list.popBack();

    solid_assert(order_list.empty());

    return 0;
}
