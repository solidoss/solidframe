#ifndef TEST_MULTIPROTOCOL_ALPHA_SERVER_HPP
#define TEST_MULTIPROTOCOL_ALPHA_SERVER_HPP

#include "frame/ipc/ipcprotocol_serialization_v1.hpp"

namespace alpha_server{

void register_messages(solid::frame::ipc::serialization_v1::Protocol &_rprotocol);

}

#endif
