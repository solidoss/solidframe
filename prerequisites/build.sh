#!/usr/bin/env bash

printUsage()
{
    echo
    echo "Usage:"
    echo
    echo "./build.sh [--all] [--boost] [--openssl] [--boringssl] [--force-download] [--debug] [--64bit] [--no-cleanup] [-h|--help]"
    echo
    echo "Examples:"
    echo
    echo "Build all external dependencies:"
    echo "$ ./build.sh"
    echo
    echo "Build all supported dependencies:"
    echo "$ ./build.sh --all"
    echo
    echo "Build only boost:"
    echo "$ ./build.sh --boost"
    echo
    echo "Build only boost with debug simbols and force download the archive:"
    echo "$ ./build.sh --boost --force-download -d"
    echo
    echo "Build boost and openssl:"
    echo "$ ./build.sh --boost --openssl"
    echo
}

BOOST_ADDR="https://dl.bintray.com/boostorg/release/1.70.0/source/boost_1_70_0.tar.bz2"
OPENSSL_ADDR="https://www.openssl.org/source/openssl-1.1.1c.tar.gz"

SYSTEM=
BIT64=
NOCLEANUP=

downloadArchive()
{
    local url="$1"
    local arc="$(basename "${url}")"
    echo "Downloading: [$arc] from [$url]"
    curl -L -O $url
}

extractTarBz2()
{
    tar -xjf "$1"
}

extractTarGz()
{
    tar -xzf "$1"
}


buildBoost()
{
    echo
    echo "Building boost..."
    echo
    BOOST_DIR=`ls . | grep "boost" | grep -v "tar"`
    echo "Cleanup previous builds..."
    rm -rf $BOOST_DIR
    rm -rf include/boost
    rm -rf lib/libboost*

    echo
    echo "Prepare the boost archive..."
    echo
    BOOST_ARCH=`find . -name "boost_*.tar.bz2" | grep -v "old/"`

    if [ -z "$BOOST_ARCH" -o -n "$DOWNLOAD" ] ; then
        mkdir old
        mv $BOOST_ARCH old/
        echo "No boost archive found - try download: $BOOST_ADDR"
        downloadArchive $BOOST_ADDR
        BOOST_ARCH=`find . -name "boost_*.tar.bz2" | grep -v "old/"`
    fi

    echo
    echo "Extracting boost [$BOOST_ARCH]..."
    echo

    extractTarBz2 $BOOST_ARCH
    BOOST_DIR=`ls . | grep "boost" | grep -v "tar"`
    echo
    echo "Making boost [$BOOST_DIR]..."
    echo

    cd "$BOOST_DIR"
    
    
    if [ $DEBUG ] ; then
        VARIANT_BUILD="debug"
    else
        VARIANT_BUILD="release"
    fi
    
    
    if		[ "$SYSTEM" = "FreeBSD" ] ; then
        sh bootstrap.sh --with-toolset=clang
        ./b2 cxxstd=14 toolset=clang --layout=system  --prefix="$EXT_DIR" --exec-prefix="$EXT_DIR" link=static threading=multi $VARIANT_BUILD install
    elif	[ "$SYSTEM" = "Darwin" ] ; then
        sh bootstrap.sh
        ./b2 cxxstd=14 --layout=system  --prefix="$EXT_DIR" --exec-prefix="$EXT_DIR" link=static threading=multi $VARIANT_BUILD install
        echo
    elif    [[ "$SYSTEM" =~ "MINGW" ]]; then
        if [ $BIT64 = true ]; then
            BOOST_ADDRESS_MODEL="64"
        else
            BOOST_ADDRESS_MODEL="32"
        fi
        
        ./bootstrap.bat
        ./b2 cxxstd=14 --abbreviate-paths --hash address-model="$BOOST_ADDRESS_MODEL" variant="$VARIANT_BUILD" link=static threading=multi --prefix="$EXT_DIR" install
    else
        sh bootstrap.sh
        ./b2 cxxstd=14 --layout=system  --prefix="$EXT_DIR" --exec-prefix="$EXT_DIR" link=static threading=multi $VARIANT_BUILD install
        echo
    fi
    
    cd ..
    if [ ! $NOCLEANUP ]; then
        rm -rf "$BOOST_DIR"
    fi
    
    echo
    echo "Done BOOST!"
    echo
}

buildOpenssl()
{
    WHAT="openssl"
    ADDR_NAME=$OPENSSL_ADDR
    echo
    echo "Building $WHAT..."
    echo

    OLD_DIR=`ls . | grep "$WHAT" | grep -v "tar"`
    echo
    echo "Cleanup previous builds..."
    echo

    rm -rf $OLD_DIR
    rm -rf include/openssl
    rm -rf lib/libssl*
    rm -rf lib64/libssl*
    rm -rf lib/libcrypto*
    rm -rf lib64/libcrypto*
    rm -rf ssl_

    echo
    echo "Prepare the $WHAT archive..."
    echo

    ARCH_NAME=`find . -name "$WHAT-*.tar.gz" | grep -v "old/"`
    if [ -z "$ARCH_NAME" -o -n "$DOWNLOAD" ] ; then
        mkdir old
        mv $ARCH_NAME old/
        echo "No $WHAT archive found or forced - try download: $ADDR_NAME"
        downloadArchive $ADDR_NAME
        ARCH_NAME=`find . -name "$WHAT-*.tar.gz" | grep -v "old/"`
    fi
    
    echo "Extracting $WHAT [$ARCH_NAME]..."
    extractTarGz $ARCH_NAME

    DIR_NAME=`ls . | grep "$WHAT" | grep -v "tar"`
    echo
    echo "Making $WHAT [$DIR_NAME]..."
    echo

    cd $DIR_NAME
    
    if  [ "$SYSTEM" = "FreeBSD" ] ; then
        if [ $DEBUG ] ; then
            CC=cc ./config --prefix="$EXT_DIR" --openssldir="ssl_"
        else
            CC=cc ./config --prefix="$EXT_DIR" --openssldir="ssl_"
        fi
    elif	[ "$SYSTEM" = "Darwin" ] ; then
        if [ $DEBUG ] ; then
            ./Configure --prefix="$EXT_DIR" --openssldir="ssl_" darwin64-x86_64-cc
        else
            ./Configure --prefix="$EXT_DIR" --openssldir="ssl_" darwin64-x86_64-cc
        fi
    elif    [[ "$SYSTEM" =~ "MINGW" ]]; then
        if [ "$BIT64" = "true" ]; then
            OPENSSL_TARGET="VC-WIN64A"
        else
            OPENSSL_TARGET="VC-WIN32"
        fi
        # on windows we need to compile the shared library too in order to avoid
        # explicit dependency of CRYPT32.LIB
        /c/Strawberry/perl/bin/perl Configure $OPENSSL_TARGET --prefix="$EXT_DIR" --openssldir="ssl_"
    else
        if [ $DEBUG ] ; then
            ./config --prefix="$EXT_DIR" --openssldir="ssl_"
        else
            ./config --prefix="$EXT_DIR" --openssldir="ssl_"
        fi
    fi
    if    [[ "$SYSTEM" =~ "MINGW" ]]; then
        nmake && nmake install_sw
    else
        make && make install_sw
    fi
    
    cd ..

    if [ ! $NOCLEANUP ]; then
        rm -rf $DIR_NAME
    fi
    
    echo
    echo "Done $WHAT!"
    echo
}

function buildBoringSSL
{
    echo
    echo "Building BoringSSL..."
    echo
    
    rm -rf include/openssl
    rm -rf lib/libssl*
    rm -rf lib64/libssl*
    rm -rf lib/libcrypto*
    rm -rf lib64/libcrypto*
    rm -rf ssl_
    
    if [ ! -d $EXT_DIR/boringssl ]; then
        git clone https://boringssl.googlesource.com/boringssl
        cd boringssl
    else
        cd boringssl
        git pull
    fi
    
    rm -rf build
    
    mkdir build
    cd build
    
    cmake -DCMAKE_INSTALL_PREFIX="$EXT_DIR" ..
    
    cmake --build . --config release
    
    if [ ! -d $EXT_DIR/include ]; then
        mkdir -p "$EXT_DIR/include"
    fi
    
    cp ssl/libssl.a "$EXT_DIR/lib"
    cp crypto/libcrypto.a "$EXT_DIR/lib"
    
    cd ..
    
    cp -r include/openssl "$EXT_DIR/include"
    
    cd "$EXT_DIR"
}

EXT_DIR="`pwd`"

BUILD_BOOST_MIN=
BUILD_BOOST_FULL=
BUILD_OPENSSL=
BUILD_BORINGSSL=

BUILD_SOMETHING=

ARCHIVE=
DOWNLOAD=
DEBUG=
HELP=


while [ "$#" -gt 0 ]; do
    CURRENT_OPT="$1"
    UNKNOWN_ARG=no
    HELP=
    case "$1" in
    --all)
        BUILD_BOOST_FULL=true
        BUILD_OPENSSL=true
        BUILD_SOMETHING=true
        ;;
    --boost)
        BUILD_BOOST_FULL=true
        BUILD_SOMETHING=true
        ;;
    --openssl)
        BUILD_OPENSSL=true
        BUILD_SOMETHING=true
        ;;
    --boringssl)
        BUILD_BORINGSSL=true
        BUILD_SOMETHING=true
        ;;
    --debug)
        DEBUG=true
        ;;
    --force-download)
        DOWNLOAD=true
        ;;
    --system)
        shift
        SYSTEM="$1"
        ;;
    --64bit)
        BIT64=true
        ;;
    --no-cleanup)
        NOCLEANUP=true
        ;;
    -h|--help)
        HELP=true
        ;;
    *)
        HELP=true
        ;;
    esac
    shift
done

if [ $HELP ]; then
    printUsage
    exit
fi

if [[ -z ${SYSTEM} ]]; then
    SYSTEM=$(uname)
fi


echo "Extern folder: $EXT_DIR"
echo "System: $SYSTEM"

if [[ ! $BUILD_SOMETHING ]]; then
    BUILD_BOOST_FULL=true
    BUILD_OPENSSL=true
fi

if [ $BUILD_OPENSSL ]; then
    buildOpenssl
fi

if [ $BUILD_BORINGSSL ]; then
    buildBoringSSL
fi

if [ $BUILD_BOOST_FULL ]; then
    buildBoost
else
    if [ $BUILD_BOOST_MIN ]; then
        buildBoost
    fi
fi


if [ -d lib64 ]; then
    cd lib

    for filename in ../lib64/*
    do
    echo "SimLink to $filename"
    ln -s $filename .
    done;
fi

if [ $ARCHIVE ]; then
    cd $EXT_DIR
    cd ../
    CWD="`basename $EXT_DIR`"
    echo $CWD

    echo "tar -cjf $CWD.tar.bz2 $CWD/include $CWD/lib $CWD/lib64 $CWD/bin $CWD/sbin $CWD/share"
    tar -cjf $CWD.tar.bz2 $CWD/include $CWD/lib $CWD/bin $CWD/sbin $CWD/share
fi

echo
echo "DONE!"
