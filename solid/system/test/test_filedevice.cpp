#include "solid/system/directory.hpp"
#include "solid/system/exception.hpp"
#include "solid/system/filedevice.hpp"
#include <array>
#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

using namespace std;
using namespace solid;

namespace {
span<const byte> as_bytes(string_view _s)
{
    return std::as_bytes(span<const char>{_s.data(), _s.size()});
}
} // namespace

int test_filedevice(int /*argc*/, char* /*argv*/[])
{
    const string dir  = "test_filedevice_dir";
    const string path = dir + "/sub/file.txt";

    filesystem::remove_all(dir);

    solid_check(Directory::create_all(dir + "/sub"));
    solid_check(Directory::create_all(dir + "/sub")); // already existing path is not an error
    {
        const auto rv = Directory::create(dir + "/sub");
        solid_check(!rv && rv.error() == errc::file_exists);
    }
    {
        FileDevice fd;
        solid_check(!fd);
        const auto rv = fd.open(dir + "/missing.txt", FileDevice::ReadOnlyE);
        solid_check(!rv && rv.error() == errc::no_such_file_or_directory);
        solid_check(!FileDevice::can_retry_open(rv.error()));
        solid_check(!fd);
    }
    {
        FileDevice fd;
        solid_check(fd.create(path, FileDevice::ReadWriteE));
        solid_check(fd);

        const string_view text = "Hello, world!";

        const auto wrv = fd.write(as_bytes(text));
        solid_check(wrv && *wrv == text.size());
        solid_check(fd.size().value() == static_cast<int64_t>(text.size()));
        solid_check(FileDevice::size(path).value() == static_cast<int64_t>(text.size()));

        solid_check(fd.seek(0).value() == 0);

        array<char, 64> buf{};
        const auto      rrv = fd.read(as_writable_bytes(span{buf}));
        solid_check(rrv && *rrv == text.size());
        solid_check(string_view(buf.data(), *rrv) == text);
        solid_check(fd.read(as_writable_bytes(span{buf})).value() == 0); // end of file

        // positional write/read
        solid_check(fd.write(as_bytes("WORLD"), 7).value() == 5);
        buf.fill(0);
        solid_check(fd.read(as_writable_bytes(span{buf}), 7).value() == text.size() - 7);
        solid_check(string_view(buf.data(), 6) == "WORLD!");

        solid_check(fd.seek(0, SeekEnd).value() == static_cast<int64_t>(text.size()));
        solid_check(fd.seek(-3, SeekCur).value() == static_cast<int64_t>(text.size() - 3));
        solid_check(fd.truncate(5));
        solid_check(fd.size().value() == 5);
        solid_check(fd.flush());
        fd.close();
        solid_check(!fd);
    }
    solid_check(FileDevice::size(path).value() == 5);
    {
        const auto rv = FileDevice::size(dir + "/missing.txt");
        solid_check(!rv && rv.error() == errc::no_such_file_or_directory);
    }

    const string path2 = dir + "/sub/file2.txt";
    solid_check(Directory::rename_file(path, path2));
    solid_check(!FileDevice::size(path));
    solid_check(FileDevice::size(path2).value() == 5);
    solid_check(Directory::erase_file(path2));
    {
        const auto rv = Directory::erase_file(path2);
        solid_check(!rv && rv.error() == errc::no_such_file_or_directory);
    }
    {
        const auto rv = Directory::rename_file(path2, path);
        solid_check(!rv && rv.error() == errc::no_such_file_or_directory);
    }

    filesystem::remove_all(dir);
    cout << "done" << endl;
    return 0;
}
