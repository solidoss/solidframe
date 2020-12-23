#pragma once

#include <cstdint>
#include "solid/utility/common.hpp"

using IndexT  = uint64_t;
using UniqueT = uint32_t;

struct UniqueId {
    IndexT  index;
    UniqueT unique;

    static UniqueId invalid()
    {
        return UniqueId();
    }

    UniqueId(
        IndexT const& _idx = solid::InvalidIndex(),
        UniqueT       _unq = solid::InvalidIndex())
        : index(_idx)
        , unique(_unq)
    {
    }

    bool isInvalid() const
    {
        return index == solid::InvalidIndex();
    }
    bool isValid() const
    {
        return !isInvalid();
    }

    bool operator==(UniqueId const& _ruid) const
    {
        return _ruid.index == this->index && _ruid.unique == this->unique;
    }
    bool operator!=(UniqueId const& _ruid) const
    {
        return _ruid.index != this->index || _ruid.unique != this->unique;
    }
    void clear()
    {
        index  = solid::InvalidIndex();
        unique = solid::InvalidIndex();
    }
};

