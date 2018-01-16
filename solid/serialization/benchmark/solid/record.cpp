#include "record.hpp"

namespace solid_test {

void to_string(Record& record, std::string& data)
{
    SerializerT s;
    to_string(s, record, data);
}
void from_string(Record& record, const std::string& data)
{
    DeserializerT d;
    from_string(d, record, data);
}

void to_string(SerializerT& _rs, Record& record, std::string& data)
{
    _rs.clear();
    data.clear();

    char buf[4096 * 4];
    int  rv;

    _rs.push(record, "record");

    while ((rv = _rs.run(buf, 4096 * 4)) > 0) {
        data.append(buf, rv);
    }
}

void from_string(DeserializerT& _rd, Record& record, const std::string& data)
{
    record.ids.clear();
    record.strings.clear();

    _rd.push(record, "record");
    _rd.run(data.data(), data.size());
}

} // namespace solid_test
