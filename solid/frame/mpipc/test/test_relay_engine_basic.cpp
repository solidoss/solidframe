#include "solid/frame/mpipc/mpipcrelayengine.hpp"

using namespace std;
using namespace solid::frame;

namespace {

Manager& manager()
{
    static Manager* pmgr = nullptr;
    return *pmgr;
}

class Test : public mpipc::RelayEngine {
public:
    Test()
        : mpipc::RelayEngine(manager())
    {
    }

    bool notifyConnection(Manager& _rm, const ObjectIdT& _rconuid, const mpipc::RelayEngineNotification _what) override
    {
        return true;
    }

    void runBasic();
};

void Test::runBasic()
{
}

} //namespace

int test_relay_engine_basic(int argc, char** argv)
{
    Test t;

    t.runBasic();

    return 0;
}
