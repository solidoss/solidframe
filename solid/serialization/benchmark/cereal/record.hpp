//courtesy to: https://github.com/thekvs/cpp-serializers

#pragma once

#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <sstream>
#include <stdint.h>
#include <string>
#include <vector>

namespace cereal_test {

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

private:
    friend class cereal::access;

    template <typename Archive>
    void serialize(Archive& ar)
    {
        ar(ids, strings);
    }
};

void to_string(const Record& record, std::string& data);
void from_string(Record& record, const std::string& data);

} // namespace cereal_test
