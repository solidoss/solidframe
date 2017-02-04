#ifndef TEST_MULTIPROTOCOL_BETA_CLIENT_HPP
#define TEST_MULTIPROTOCOL_BETA_CLIENT_HPP

#include "../../clientcommon.hpp"

namespace beta_client {

solid::ErrorConditionT start(
    Context& _rctx);

void stop();
}

#endif
