#ifndef TEST_MULTIPROTOCOL_ALPHA_CLIENT_HPP
#define TEST_MULTIPROTOCOL_ALPHA_CLIENT_HPP

#include "../../clientcommon.hpp"

namespace alpha_client {

solid::ErrorConditionT start(
    Context& _rctx);

void stop();
}

#endif
