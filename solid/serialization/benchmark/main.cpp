//courtesy to: https://github.com/thekvs/cpp-serializers

#include <chrono>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>

#include <boost/lexical_cast.hpp>

#include "boost/record.hpp"
#include "cereal/record.hpp"
#include "data.hpp"
#include "solid/record.hpp"

using namespace std;

void boost_serialization_test(size_t iterations)
{
    using namespace boost_test;

    Record r1, r2;

    for (size_t i = 0; i < kIntegers.size(); i++) {
        r1.ids.push_back(kIntegers[i]);
    }

    for (size_t i = 0; i < kStringsCount; i++) {
        r1.strings.push_back(kStringValue);
    }

    std::string serialized;

    to_string(r1, serialized);
    from_string(r2, serialized);

    if (r1 != r2) {
        throw std::logic_error("boost's case: deserialization failed");
    }

    std::cout << "boost: version = " << BOOST_VERSION << std::endl;
    std::cout << "boost: size = " << serialized.size() << " bytes" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; i++) {
        serialized.clear();
        to_string(r1, serialized);
        from_string(r2, serialized);
    }
    auto finish   = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();

    std::cout << "boost: time = " << duration << " milliseconds" << std::endl
              << std::endl;
}

void cereal_serialization_test(size_t iterations)
{
    using namespace cereal_test;

    Record r1, r2;

    for (size_t i = 0; i < kIntegers.size(); i++) {
        r1.ids.push_back(kIntegers[i]);
    }

    for (size_t i = 0; i < kStringsCount; i++) {
        r1.strings.push_back(kStringValue);
    }

    std::string serialized;

    to_string(r1, serialized);
    from_string(r2, serialized);

    if (r1 != r2) {
        throw std::logic_error("cereal's case: deserialization failed");
    }

    std::cout << "cereal: size = " << serialized.size() << " bytes" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < iterations; i++) {
        serialized.clear();
        to_string(r1, serialized);
        from_string(r2, serialized);
    }
    auto finish   = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();

    std::cout << "cereal: time = " << duration << " milliseconds" << std::endl
              << std::endl;
}

void solid_serialization_test(size_t iterations)
{
    using namespace solid_test;

    Record r1, r2;

    for (size_t i = 0; i < kIntegers.size(); i++) {
        r1.ids.push_back(kIntegers[i]);
    }

    for (size_t i = 0; i < kStringsCount; i++) {
        r1.strings.push_back(kStringValue);
    }

    SerializerT   s;
    DeserializerT d;

    std::string serialized;

    to_string(s, r1, serialized);
    from_string(d, r2, serialized);

    if (r1 != r2) {
        throw std::logic_error("solid's case: deserialization failed");
    }

    std::cout << "solid: size = " << serialized.size() << " bytes" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();
    //char buf[256];
    for (size_t i = 0; i < iterations; i++) {
        serialized.clear();
        //s.pushNoop(25102510);
        //s.run(buf, 256);
        //d.pushNoop(25072507);
        //d.run(buf, 256);
        to_string(s, r1, serialized);
        from_string(d, r2, serialized);
    }
    auto finish   = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();

#ifdef SOLID_HAS_STATISTICS
    cout << "Serialization statistics  :" << endl
         << s.statistics() << endl;
    cout << "Deserialization statistics:" << endl
         << d.statistics() << endl;
#endif

    std::cout << "solid: time = " << duration << " milliseconds" << std::endl
              << std::endl;
}

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cout << "usage: " << argv[0] << " N [boost solid cereal]";
        std::cout << std::endl
                  << std::endl;
        std::cout << "arguments: " << std::endl;
        std::cout << " N  -- number of iterations" << std::endl
                  << std::endl;
        return EXIT_SUCCESS;
    }

    size_t iterations;

    try {
        iterations = boost::lexical_cast<size_t>(argv[1]);
    } catch (std::exception& exc) {
        std::cerr << "Error: " << exc.what() << std::endl;
        std::cerr << "First positional argument must be an integer." << std::endl;
        return EXIT_FAILURE;
    }

    std::set<std::string> names;

    if (argc > 2) {
        for (int i = 2; i < argc; i++) {
            names.insert(argv[i]);
        }
    }

    std::cout << "performing " << iterations << " iterations" << std::endl
              << std::endl;

    try {

        if (names.empty() || names.find("boost") != names.end()) {
            boost_serialization_test(iterations);
        }

        if (names.empty() || names.find("solid") != names.end()) {
            solid_serialization_test(iterations);
        }

        if (names.empty() || names.find("cereal") != names.end()) {
            cereal_serialization_test(iterations);
        }
    } catch (std::exception& exc) {
        std::cerr << "Error: " << exc.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
