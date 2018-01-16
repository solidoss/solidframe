//courtesy to: https://github.com/thekvs/cpp-serializers
#include "record.hpp"
#include "cereal/archives/binary.hpp"

namespace cereal_test {

void to_string(const Record& record, std::string& data)
{
    std::ostringstream stream;
    //boost::archive::binary_oarchive archiver(stream, boost::archive::no_header | boost::archive::no_codecvt | boost::archive::no_tracking);
    cereal::BinaryOutputArchive archiver(stream);
    archiver << record;

    data = stream.str();
}

void from_string(Record& record, const std::string& data)
{
    std::stringstream stream(data);
    //boost::archive::binary_iarchive archiver(stream, boost::archive::no_header | boost::archive::no_codecvt | boost::archive::no_tracking);
    cereal::BinaryInputArchive archiver(stream);
    archiver >> record;
}

} // namespace cereal_test
