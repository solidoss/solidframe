
#pragma once

#include <sstream>
#include <string>
#include <vector>

#include <stdint.h>

#include "solid/serialization/binary.hpp"

namespace solid_test {

typedef std::vector<int64_t>     Integers;
typedef std::vector<std::string> Strings;

class Record {
public:
    Integers ids;
    Strings  strings;

    bool operator==(const Record& other)
    {
        return (ids == other.ids && strings == other.strings);
    }

    bool operator!=(const Record& other)
    {
        return !(*this == other);
    }

    template <class S>
    void solidSerialize(S& _s)
    {
        _s.pushContainer(ids, "Record::ids");
        _s.pushContainer(strings, "Record::strings");
    }
};

using SerializerT   = solid::serialization::binary::Serializer<void>;
using DeserializerT = solid::serialization::binary::Deserializer<void>;
using TypeIdMapT    = solid::serialization::TypeIdMap<SerializerT, DeserializerT>;

void to_string(Record& record, std::string& data);
void from_string(Record& record, const std::string& data);

void to_string(SerializerT& _rs, Record& record, std::string& data);
void from_string(DeserializerT& _rd, Record& record, const std::string& data);

} // namespace solid_test
