#include "solid/system/chunkedstream.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/log.hpp"
#include <chrono>
#include <iostream>
#include <sstream>
using namespace solid;
using namespace std;

namespace {
bool read(string& _rs, istream& _ris, size_t _sz)
{
    static constexpr size_t bufcp = 1024 * 2;
    char                    buf[bufcp];
    while (!_ris.eof() && _sz != 0) {
        size_t toread = bufcp;
        if (toread > _sz) {
            toread = _sz;
        }

        _ris.read(buf, toread);
        _rs.append(buf, toread);
        _sz -= toread;
    }
    return _sz == 0;
}

} // namespace

int test_chunkedstream(int argc, char* argv[])
{

    using OChunkedStreamT = OChunkedStream<4 * 1024>;
    using IChunkedStreamT = IChunkedStream<4 * 1024>;

    const size_t count = 1000 * 1000 * 10;

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    if (1) {
        OChunkedStreamT ocs;
        for (size_t i = 0; i < count; ++i) {
            ocs << i << ' ';
        }
        IChunkedStreamT ics(std::move(ocs));
        for (size_t i = 0; i < count; ++i) {
            size_t v;
            ics >> v;
            solid_check(v == i, "values differ: " << i << " vs " << v);
        }
    }
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    cout << "chunkedstream took: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << endl;

    {
        OChunkedStreamT ocs;
        for (size_t i = 0; i < count; ++i) {
            ocs << i << ' ';
        }

        size_t sz = ocs.size();
        string s;
        {
            IChunkedStreamT ics(std::move(ocs));

            read(s, ics, sz);
        }

        istringstream iss(s);

        for (size_t i = 0; i < count; ++i) {
            size_t v;
            iss >> v;
            solid_check(v == i, "values differ: " << i << " vs " << v);
        }
    }

#if 0
    start = std::chrono::steady_clock::now();
    {
        ostringstream oss;
        for (size_t i = 0; i < count; ++i) {
            oss << i << ' ';
        }
#if 1
        istringstream iss(oss.str());
        
        for (size_t i = 0; i < count; ++i) {
            size_t v;
            iss >> v;
            solid_check(v == i, "values differ: " << i << " vs " << v);
        }
#endif
    }
    end = std::chrono::steady_clock::now();
    cout<<"stringstream took: "<<std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()<<endl;
#endif
    return 0;
}
