#include "solid/frame/mprpc/mprpcrelayengine.hpp"

using namespace std;
using namespace solid::frame;

namespace {

class Test : public mprpc::RelayEngine {
public:
    Test()
    {
    }

    bool notifyConnection(Manager& _rm, const ActorIdT& _rconuid, const mprpc::RelayEngineNotification _what) override
    {
        return true;
    }

    void runBasic();
};

void Test::runBasic()
{
}

} //namespace

int test_relay_engine_basic(int argc, char* argv[])
{
    Test t;

    t.runBasic();

    return 0;
}
