// solid/serialization/src/typeidmap.cpp
//
// Copyright (c) 2014 Valentin Palade (vipalade @ gmail . com)
//
// This file is part of SolidFrame framework.
//
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt.
//

#include "solid/serialization/typeidmap.hpp"
#include "solid/utility/algorithm.hpp"

namespace solid {
namespace serialization {

class ErrorCategory : public ErrorCategoryT {
public:
    enum {
        NoTypeE = 1,
        NoCastE,
    };
    ErrorCategory() {}

private:
    const char* name() const noexcept(true)
    {
        return "solid::serialization::TypeIdMap";
    }

    std::string message(int _ev) const
    {
        switch (_ev) {
        case NoTypeE:
            return "Type not registered";
        case NoCastE:
            return "Cast not registered";
        default:
            return "Unknown";
        }
    }
};

const ErrorCategory ec;

/*static*/ ErrorConditionT TypeIdMapBase::error_no_cast()
{
    return ErrorConditionT(ErrorCategory::NoCastE, ec);
}
/*static*/ ErrorConditionT TypeIdMapBase::error_no_type()
{
    return ErrorConditionT(ErrorCategory::NoTypeE, ec);
}

//2 bits - the number of bytes to store _protocol_id
//   * 0 - the first byte (without the first 2 bits) stores the _protocol_id
//   * 1 - the first 2 bytes (without the first 2 bits) stores the _protocol_id
//the rest is for storing the _message_id

bool joinTypeId(uint64_t& _rtype_id, const uint32_t _protocol_id, const uint64_t _message_id)
{
    size_t proto_bit_count = max_bit_count(_protocol_id);

    if (proto_bit_count > 30)
        return false;

    const size_t proto_byte_count = (fast_padded_size(proto_bit_count + 2, 3) >> 3);
    const size_t msgid_byte_count = max_padded_byte_cout(_message_id);

    if (proto_byte_count + msgid_byte_count > sizeof(uint64_t)) {
        return false;
    }
    _rtype_id = ((proto_byte_count - 1) & 0x3) | (_protocol_id << 2) | (_message_id << (proto_byte_count * 8));
    return true;
}

void splitTypeId(const uint64_t _type_id, uint32_t& _rprotocol_id, uint64_t& _rmessage_id)
{
    static const uint32_t mask[]           = {0xff, 0xffff, 0xffffff, 0xffffffff};
    size_t                proto_byte_count = _type_id & 0x3;
    _rprotocol_id                          = (_type_id & mask[proto_byte_count]) >> 2;
    _rmessage_id                           = (_type_id >> ((proto_byte_count + 1) * 8));
}

bool TypeIdMapBase::findTypeIndex(const uint64_t& _rid, size_t& _rindex) const
{
    auto it = msgidmap.find(_rid);
    if (it != msgidmap.end()) {
        _rindex = it->second;
        return true;
    }
    return false;
}

size_t TypeIdMapBase::doAllocateNewIndex(const size_t _protocol_id, uint64_t& _rid)
{

    auto protoit = protomap.find(_protocol_id);

    if (protoit != protomap.end()) {
    } else {
        protomap[_protocol_id] = ProtocolStub();
    }
    ProtocolStub& rps = protomap[_protocol_id];

    size_t rv = InvalidIndex();

    do {

        if (joinTypeId(_rid, static_cast<uint32_t>(_protocol_id), rps.current_message_index)) {
            ++rps.current_message_index;
        } else {
            rv = InvalidIndex();
            break;
        }
    } while (findTypeIndex(_rid, rv));
    return rv;
}

bool TypeIdMapBase::doFindTypeIndex(const size_t _protocol_id, size_t _idx, uint64_t& _rid) const
{
    if (joinTypeId(_rid, static_cast<uint32_t>(_protocol_id), _idx)) {
        size_t index;
        return findTypeIndex(_rid, index);
    }
    return false;
}

} //namespace serialization
} //namespace solid
