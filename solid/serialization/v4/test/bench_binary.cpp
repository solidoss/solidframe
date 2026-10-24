// Serialization v3 vs v4 throughput on identical wire data.
// usage: bench_serialization_v4 [iterations_scale]

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "solid/serialization/v3/serialization.hpp"
#include "solid/serialization/v4/serialization.hpp"
#include "solid/system/exception.hpp"

using namespace std;
using namespace solid;
using namespace std::literals;

namespace {

struct Context {};

using SerializerV3T   = serialization::v3::binary::Serializer<reflection::v1::metadata::Variant<Context>, decltype(reflection::v1::metadata::factory), Context, uint8_t>;
using DeserializerV3T = serialization::v3::binary::Deserializer<reflection::v1::metadata::Variant<Context>, decltype(reflection::v1::metadata::factory), Context, uint8_t>;
using SerializerV4T   = serialization::v4::binary::Serializer<Context, uint8_t>;
using DeserializerV4T = serialization::v4::binary::Deserializer<Context, uint8_t>;

// v3 runs on a pointer and a size, v4 on a byte span.
template <class Ser, class... Args>
ptrdiff_t run_ser(Ser& _rser, vector<char>& _rbuf, Args&&... _args)
{
    if constexpr (std::is_same_v<Ser, SerializerV3T>) {
        return _rser.run(_rbuf.data(), static_cast<unsigned>(_rbuf.size()), std::forward<Args>(_args)...);
    } else {
        return _rser.run(as_writable_bytes(span{_rbuf}), std::forward<Args>(_args)...);
    }
}

template <class Des, class... Args>
ptrdiff_t run_des(Des& _rdes, string const& _data, const size_t _offset, const size_t _sz, Args&&... _args)
{
    if constexpr (std::is_same_v<Des, DeserializerV3T>) {
        return _rdes.run(_data.data() + _offset, _sz, std::forward<Args>(_args)...);
    } else {
        return _rdes.run(as_bytes(span{_data}).subspan(_offset, _sz), std::forward<Args>(_args)...);
    }
}

struct Item {
    uint64_t id_    = 0;
    uint32_t count_ = 0;
    bool     flag_  = false;
    string   name_;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.id_, _rctx, 1, "id").add(_rthis.count_, _rctx, 2, "count").add(_rthis.flag_, _rctx, 3, "flag").add(_rthis.name_, _rctx, 4, "name");
    }

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("id"sv, rthis.id_), _rc);
        _rr(id<2>, make("count"sv, rthis.count_), _rc);
        _rr(id<3>, make("flag"sv, rthis.flag_), _rc);
        _rr(id<4>, make("name"sv, rthis.name_), _rc);
    }
};

// A typical request: a few scalars, strings and a list of small records.
struct Request {
    uint32_t              version_ = 0;
    uint64_t              session_ = 0;
    string                user_;
    string                path_;
    vector<Item>          items_;
    map<string, uint64_t> attributes_;
    vector<uint32_t>      ids_;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.version_, _rctx, 1, "version").add(_rthis.session_, _rctx, 2, "session");
        _rr.add(_rthis.user_, _rctx, 3, "user").add(_rthis.path_, _rctx, 4, "path");
        _rr.add(_rthis.items_, _rctx, 5, "items").add(_rthis.attributes_, _rctx, 6, "attributes").add(_rthis.ids_, _rctx, 7, "ids");
    }

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("version"sv, rthis.version_), _rc);
        _rr(id<2>, make("session"sv, rthis.session_), _rc);
        _rr(id<3>, make("user"sv, rthis.user_), _rc);
        _rr(id<4>, make("path"sv, rthis.path_), _rc);
        _rr(id<5>, make("items"sv, rthis.items_), _rc);
        _rr(id<6>, make("attributes"sv, rthis.attributes_), _rc);
        _rr(id<7>, make("ids"sv, rthis.ids_), _rc);
    }
};

// Bulk binary payload.
struct Blob {
    string          text_;
    vector<uint8_t> bytes_;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.text_, _rctx, 1, "text").add(_rthis.bytes_, _rctx, 2, "bytes");
    }

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("text"sv, rthis.text_), _rc);
        _rr(id<2>, make("bytes"sv, rthis.bytes_), _rc);
    }
};

struct Shape {
    uint32_t id_ = 0;
    virtual ~Shape() {}
};

struct Circle : Shape {
    uint32_t radius_ = 0;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.id_, _rctx, 1, "id").add(_rthis.radius_, _rctx, 2, "radius");
    }

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("id"sv, rthis.id_), _rc);
        _rr(id<2>, make("radius"sv, rthis.radius_), _rc);
    }
};

struct Label : Shape {
    string text_;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.id_, _rctx, 1, "id").add(_rthis.text_, _rctx, 2, "text");
    }

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("id"sv, rthis.id_), _rc);
        _rr(id<2>, make("text"sv, rthis.text_), _rc);
    }
};

// Polymorphic objects: exercises the typemaps (v1 for v3, v2 for v4).
struct Drawing {
    vector<shared_ptr<Shape>> shapes_;

    SOLID_REFLECT_V1(_rr, _rthis, _rctx)
    {
        _rr.add(_rthis.shapes_, _rctx, 1, "shapes");
    }

    template <typename Reflector, typename Ctx>
    void solidReflectV2(this auto& rthis, Reflector&& _rr, Ctx& _rc)
    {
        using namespace solid::reflection::v2;
        _rr(id<1>, make("shapes"sv, rthis.shapes_), _rc);
    }
};

reflection::v1::TypeMap<SerializerV3T, DeserializerV3T> const& type_map_v3()
{
    static const reflection::v1::TypeMap<SerializerV3T, DeserializerV3T> type_map{[](auto& _rmap) {
        _rmap.template registerType<Circle, Shape>(0, 1, "Circle");
        _rmap.template registerType<Label, Shape>(0, 2, "Label");
    }};
    return type_map;
}

reflection::v2::TypeMap<SerializerV4T, DeserializerV4T> const& type_map_v4()
{
    static const reflection::v2::TypeMap<SerializerV4T, DeserializerV4T> type_map{[](auto& _rmap) {
        _rmap.template register_type<Circle, Shape>(reflection::v2::TypeId{1}, reflection::v2::TypeName{"Circle"});
        _rmap.template register_type<Label, Shape>(reflection::v2::TypeId{2}, reflection::v2::TypeName{"Label"});
    }};
    return type_map;
}

Drawing make_drawing(const size_t _count)
{
    Drawing drawing;
    for (size_t i = 0; i < _count; ++i) {
        if ((i % 2) == 0) {
            auto circle     = std::make_shared<Circle>();
            circle->id_     = static_cast<uint32_t>(i);
            circle->radius_ = static_cast<uint32_t>(i * 3);
            drawing.shapes_.push_back(circle);
        } else {
            auto label   = std::make_shared<Label>();
            label->id_   = static_cast<uint32_t>(i);
            label->text_ = "label " + std::to_string(i);
            drawing.shapes_.push_back(label);
        }
    }
    return drawing;
}

Request make_request(const size_t _items)
{
    Request req;
    req.version_ = 3;
    req.session_ = 0x1234567890ULL;
    req.user_    = "some.user@example.com";
    req.path_    = "/storage/folder/subfolder/file.txt";
    for (size_t i = 0; i < _items; ++i) {
        req.items_.push_back(Item{i * 1000003, static_cast<uint32_t>(i), (i % 2) == 0, "item_" + std::to_string(i)});
    }
    for (size_t i = 0; i < 16; ++i) {
        req.attributes_["attribute_" + std::to_string(i)] = i * 77;
    }
    for (size_t i = 0; i < 64; ++i) {
        req.ids_.push_back(static_cast<uint32_t>(i * 31));
    }
    return req;
}

template <class T>
string serialize_v3(T const& _rt, vector<char>& _rbuf)
{
    Context       ctx;
    SerializerV3T ser{reflection::v1::metadata::factory, type_map_v3()};
    string        out;
    for (ptrdiff_t rv = ser.run(_rbuf.data(), static_cast<unsigned>(_rbuf.size()), [&_rt](SerializerV3T& _rs, Context& _rctx) { _rs.add(_rt, _rctx, 0, "t"); }, ctx); rv > 0; rv = ser.run(_rbuf.data(), static_cast<unsigned>(_rbuf.size()), ctx)) {
        out.append_range(span{_rbuf}.first(static_cast<size_t>(rv)));
    }
    return out;
}

template <class T>
string serialize_v4(T const& _rt, vector<char>& _rbuf)
{
    Context       ctx;
    SerializerV4T ser{type_map_v4()};
    string        out;
    for (ptrdiff_t rv = ser.run(as_writable_bytes(span{_rbuf}), [&_rt](SerializerV4T& _rs, Context& _rctx) { _rs.add(_rt, _rctx); }, ctx); rv > 0; rv = ser.run(as_writable_bytes(span{_rbuf}), ctx)) {
        out.append_range(span{_rbuf}.first(static_cast<size_t>(rv)));
    }
    return out;
}

// Serialization only: output buffers are written and dropped, like packets.
template <class Ser, class T>
size_t run_serialize(T const& _rt, vector<char>& _rbuf)
{
    Context ctx;
    Ser     ser = [] {
        if constexpr (std::is_same_v<Ser, SerializerV3T>) {
            return SerializerV3T{reflection::v1::metadata::factory, type_map_v3()};
        } else {
            return SerializerV4T{type_map_v4()};
        }
    }();
    size_t total = 0;
    auto   add   = [&_rt](Ser& _rs, Context& _rctx) {
        if constexpr (std::is_same_v<Ser, SerializerV3T>) {
            _rs.add(_rt, _rctx, 0, "t");
        } else {
            _rs.add(_rt, _rctx);
        }
    };
    for (ptrdiff_t rv = run_ser(ser, _rbuf, add, ctx); rv > 0; rv = run_ser(ser, _rbuf, ctx)) {
        total += static_cast<size_t>(rv);
    }
    return total;
}

template <class Des, class T>
size_t run_deserialize(string const& _data, const size_t _chunk, T& _rt)
{
    Context ctx;
    Des     des = [] {
        if constexpr (std::is_same_v<Des, DeserializerV3T>) {
            return DeserializerV3T{reflection::v1::metadata::factory, type_map_v3()};
        } else {
            return DeserializerV4T{type_map_v4()};
        }
    }();
    auto add = [&_rt](Des& _rd, Context& _rctx) {
        if constexpr (std::is_same_v<Des, DeserializerV3T>) {
            _rd.add(_rt, _rctx, 0, "t");
        } else {
            _rd.add(_rt, _rctx);
        }
    };
    size_t    offset = 0;
    ptrdiff_t rv     = run_des(des, _data, 0, std::min(_chunk, _data.size()), add, ctx);
    while (rv > 0) {
        offset += static_cast<size_t>(rv);
        if (offset == _data.size()) {
            break;
        }
        rv = run_des(des, _data, offset, std::min(_chunk, _data.size() - offset), ctx);
    }
    return offset;
}

template <class F>
double measure_ns(const size_t _iterations, F&& _fnc)
{
    const auto start = chrono::steady_clock::now();
    for (size_t i = 0; i < _iterations; ++i) {
        _fnc();
    }
    const auto end = chrono::steady_clock::now();
    return static_cast<double>(chrono::duration_cast<chrono::nanoseconds>(end - start).count()) / static_cast<double>(_iterations);
}

template <class T>
void bench(const char* _name, T const& _rt, const size_t _chunk, const size_t _iterations)
{
    vector<char> buf(_chunk);
    const string data_v3 = serialize_v3(_rt, buf);
    const string data_v4 = serialize_v4(_rt, buf);
    solid_check(data_v3 == data_v4, "wire data differs for " << _name);

    volatile size_t sink = 0;
    // warm up
    sink = sink + run_serialize<SerializerV3T>(_rt, buf) + run_serialize<SerializerV4T>(_rt, buf);

    const double ser_v3 = measure_ns(_iterations, [&] { sink = sink + run_serialize<SerializerV3T>(_rt, buf); });
    const double ser_v4 = measure_ns(_iterations, [&] { sink = sink + run_serialize<SerializerV4T>(_rt, buf); });
    const double des_v3 = measure_ns(_iterations, [&] { T t; sink = sink + run_deserialize<DeserializerV3T>(data_v3, _chunk, t); });
    const double des_v4 = measure_ns(_iterations, [&] { T t; sink = sink + run_deserialize<DeserializerV4T>(data_v4, _chunk, t); });

    auto mbps = [&data_v3](const double _ns) {
        return static_cast<double>(data_v3.size()) / _ns * 1e9 / (1024.0 * 1024.0);
    };
    cout << left << setw(22) << _name << right << setw(9) << data_v3.size() << setw(7) << _chunk << fixed << setprecision(0)
         << " | ser v3 " << setw(9) << ser_v3 << "ns " << setw(6) << mbps(ser_v3) << "MB/s"
         << " v4 " << setw(9) << ser_v4 << "ns " << setw(6) << mbps(ser_v4) << "MB/s" << setprecision(2) << " x" << ser_v3 / ser_v4 << setprecision(0)
         << " | des v3 " << setw(9) << des_v3 << "ns " << setw(6) << mbps(des_v3) << "MB/s"
         << " v4 " << setw(9) << des_v4 << "ns " << setw(6) << mbps(des_v4) << "MB/s" << setprecision(2) << " x" << des_v3 / des_v4 << endl;
}

} // namespace

int main(int argc, char* argv[])
{
    const size_t scale = argc > 1 ? static_cast<size_t>(std::atol(argv[1])) : 1;

    cout << left << setw(22) << "case" << right << setw(9) << "bytes" << setw(7) << "buffer" << endl;

    const Request small = make_request(4);
    const Request large = make_request(1000);
    Blob          blob;
    blob.text_.assign(1024 * 1024, 'x');
    blob.bytes_.assign(1024 * 1024, 0x5A);

    const Drawing drawing = make_drawing(1000);

    for (const size_t chunk : {size_t{4096}, size_t{65536}}) {
        bench("request(4 items)", small, chunk, 20000 * scale);
        bench("request(1000 items)", large, chunk, 200 * scale);
        bench("polymorphic(1000)", drawing, chunk, 200 * scale);
        bench("blob(2MB)", blob, chunk, 50 * scale);
    }
    return 0;
}
