# SolidFrame Releases

## Version 6.0

* (DONE) rename mpipc library to mprpc
* (DONE) remove boost dependency
* (DONE) Object -> Actor
* (DONE) Overal fixes
* (DONE) Refactored solid::WorkPool<>. solid::CallPool<>

## Version 5.0

* (DONE) add support for endianess on seralization/v2
* (DONE) clang tidy support
* (DONE) fix compilation on g++ 8.1.1
* (DONE) system/debug.hpp -> system/log.hpp - redesign debug logging engine. No locking while handling log line parameters.
* (DONE) utility/workpool.hpp -> improved locking for a better performance on macOS
* (DONE) mpipc: call connection pool close callback after calling connection close callback for every connection in the pool
* (DONE) mpipc: improve connection pool with support for events like ConnectionActivated, PoolDisconnect, ConnectionStop

## Version 4.0

* (DONE) port to Windows
* (DONE) SolidFrame.podspec
* (DONE) solid_frame_mpipc: messagereader - cache deserializer
* (DONE) solid_frame_mpipc: messagewriter - cache serializer
* (DONE) solid_serialization_v2 - improved binary serialization engine

## Version 3.0

* (DONE) solid_frame_mpipc: improve protocol to allow transparent (i.e. not knowing the type of the mesage) message handling - e.g. skipping, relaying.
* (DONE) solid_frame_mpipc: support for generic message relaying: solid::frame::mpipc::relay::Engine. 
* (DONE) tutorials: mpipc_echo_relay
* (DONE) solid_system: improve SOLID_CHECK and SOLID_THROW
* (DONE) solid_system: pimpl.h with make_pimpl support
* (DONE) solid_utility: fix and improve solid::inner::List
* (DONE) solid_utility: delegate.h
* (DONE) integrate Travis support
* (DONE) integrate clangformat support

## Version 2.2

* (DONE) all: switch from NanoTime to std::chrono
* (DONE) BUILD: cmake support for clangformat
* (DONE) solid_serialization: speed improvements, plain arrays serialization

## Version 2.1

* (DONE) solid_frame_aio_openssl: Improved OpenSSL/BoringSSL support
* (DONE) solid_frame_mpipc: Pluggable (header only) support for SSL
* (DONE) BUILD: Android support - https://github.com/vipalade/bubbles
* (DONE) solid_serialization: Support for std::bitset, std::vector<bool> and std::set
* (DONE) solid_serialization: Test suported stl containers.
* (DONE) BUILD: Support for CMake extern command for find_package(SolidFrame)
* (DONE) solid_frame_mpipc: Basic, pluggable compression support using [Snappy](https://google.github.io/snappy/)
* (DONE) TUTORIAL: mpipc_request_ssl


## Version 2.0

* Fix utility memoryfile examples
* pushCall instead of pushReinit
* Finalizing the MPRPC library.
* Cross-platform support:
    * Linux
    * FreeBSD
    * Darwin/OSX (starting with XCode 8 for thread_local)
* Add "make install" support.
* Documentation.

