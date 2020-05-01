#pragma once

#include "solid/system/version_impl.hpp"

namespace solid{

const char * version_vcs_commit();
const char * version_vcs_branch();
const char * version_full();

}//namespace solid
