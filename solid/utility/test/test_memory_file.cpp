#include "solid/system/common.hpp"
#include "solid/system/exception.hpp"
#include "solid/utility/memoryfile.hpp"
#include <cerrno>
#include <cstring>
#include <ctime>
#include <iostream>
#include <vector>

using namespace std;
using namespace solid;
namespace {
std::string pattern;

size_t real_size(size_t _sz)
{
    //offset + (align - (offset mod align)) mod align
    return _sz + ((sizeof(uint64_t) - (_sz % sizeof(uint64_t))) % sizeof(uint64_t));
}

void init(string& _rostr, size_t _sz)
{
    const size_t sz = real_size(_sz);
    _rostr.resize(sz);
    const size_t    count        = sz / sizeof(uint64_t);
    uint64_t*       pu           = reinterpret_cast<uint64_t*>(const_cast<char*>(_rostr.data()));
    const uint64_t* pup          = reinterpret_cast<const uint64_t*>(pattern.data());
    const size_t    pattern_size = pattern.size() / sizeof(uint64_t);
    for (uint64_t i = 0; i < count; ++i) {
        pu[i] = pup[(i) % pattern_size]; //pattern[i % pattern.size()];
    }
}

void test_read_write(size_t _sz, const std::vector<size_t>& _write_size_vec, const std::vector<size_t>& _read_size_vec)
{

    MemoryFile mf;

    std::string instr;
    {
        init(instr, _sz);
        size_t idx = 0;
        size_t off = 0;
        do {
            int to_write = static_cast<int>(_write_size_vec[idx % _write_size_vec.size()]);
            if (to_write > static_cast<int>(instr.size() - off)) {
                to_write = static_cast<int>(instr.size() - off);
            }
            int rv = mf.write(instr.data() + off, to_write);
            solid_check(rv == to_write);
            off += to_write;
            ++idx;
        } while (off < instr.size());
    }

    solid_check(mf.size() == static_cast<int64_t>(instr.size()));
    mf.seek(0, SeekBeg);

    {
        std::string   outstr;
        size_t        idx    = 0;
        constexpr int buf_cp = 10 * 1024;
        char          buf[10 * 1024];
        bool          fwd;
        do {

            int to_read = static_cast<int>(_read_size_vec[idx % _read_size_vec.size()]);
            solid_check(to_read < buf_cp);
            int rv = mf.read(buf, to_read);
            if (rv > 0) {
                solid_check(rv <= to_read);
                outstr.append(buf, rv);
            }
            fwd = (rv == to_read);
        } while (fwd);

        solid_check(outstr == instr);
    }
}

} //namespace

int test_memory_file(int argc, char* argv[])
{
    for (int j = 0; j < 1; ++j) {
        for (int i = 0; i < 127; ++i) {
            int c = (i + j) % 127;
            if (isprint(c) != 0 && isblank(c) == 0) {
                pattern += static_cast<char>(c);
            }
        }
    }

    int size = 1000000;

    if (argc >= 2) {
        size = atoi(argv[1]);
    }

    const std::vector<size_t> write_size_vec = {10, 100, 1000, 2000, 4000, 6000, 8000, 10000};
    const std::vector<size_t> read_size_vec  = {11, 111, 1111, 2222, 44444, 6666, 8888, 11111};

    test_read_write(size, write_size_vec, read_size_vec);
    return 0;
}
