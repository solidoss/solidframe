#ifndef TEST_MULTIPROTOCOL_GAMMA_SERVER_HPP
#define TEST_MULTIPROTOCOL_GAMMA_SERVER_HPP

#include "solid/frame/mpipc/mpipcprotocol_serialization_v1.hpp"

namespace gamma_server{

void register_messages(solid::frame::mpipc::serialization_v1::Protocol &_rprotocol);

}

#endif
