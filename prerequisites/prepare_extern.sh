#!/usr/bin/env bash

printUsage()
{
	echo
	echo "Usage:"
	echo
	echo "./prepare_extern.sh [--all] [--boost] [--openssl] [--force-download] [--debug] [-h|--help]"
	echo
	echo "Examples:"
	echo
	echo "Build SolidFrame dependencies:"
	echo "$ ./prepare_extern.sh"
	echo
	echo "Build all supported dependencies:"
	echo "$ ./prepare_extern.sh --all"
	echo
	echo "Build only boost:"
	echo "$ ./prepare_extern.sh --boost"
	echo
	echo "Build only boost with debug simbols and force download the archive:"
	echo "$ ./prepare_extern.sh --boost --force-download -d"
	echo
	echo "Build boost and openssl:"
	echo "$ ./prepare_extern.sh --boost --openssl"
	echo
}

BOOST_ADDR="http://sourceforge.net/projects/boost/files/boost/1.61.0/boost_1_61_0.tar.bz2"
OPENSSL_ADDR="https://www.openssl.org/source/openssl-1.0.2h.tar.gz"
LEVELDB_ADDR="https://leveldb.googlecode.com/files/leveldb-1.15.0.tar.gz"
SNAPPY_ADDR="http://snappy.googlecode.com/files/snappy-1.0.5.tar.gz"

SYSTEM=

downloadArchive()
{
	local url="$1"
	local arc="$(basename "${url}")"
	echo "Downloading: [$arc] from [$url]"
	#wget --no-check-certificate -O $arc $url
	#wget -O $arc $url
	curl -L -O $url
}

extractTarBz2()
{
#    bzip2 -dc "$1" | tar -xf -
	tar -xjf "$1"
}

extractTarGz()
{
#    gzip -dc "$1" | tar -xf -
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
		VARIANT_BUILD="variant=debug"
	else
		VARIANT_BUILD="variant=release"
	fi
	
	
	if		[ "$SYSTEM" = "FreeBSD" ] ; then
		sh bootstrap.sh --with-toolset=clang
		./b2 toolset=clang --layout=system  --prefix="$EXT_DIR" --exec-prefix="$EXT_DIR" link=static threading=multi $VARIANT_BUILD install
	elif	[ "$SYSTEM" = "Darwin" ] ; then
		sh bootstrap.sh
		./b2 --layout=system  --prefix="$EXT_DIR" --exec-prefix="$EXT_DIR" link=static threading=multi $VARIANT_BUILD install
		echo
	else
		sh bootstrap.sh
		./b2 --layout=system  --prefix="$EXT_DIR" --exec-prefix="$EXT_DIR" link=static threading=multi $VARIANT_BUILD install
		echo
	fi
	
	
	echo
	echo "Done BOOST!"
	echo
	cd ..
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
	
	if		[ "$SYSTEM" = "FreeBSD" ] ; then
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
	else
		if [ $DEBUG ] ; then
			./config --prefix="$EXT_DIR" --openssldir="ssl_"
		else
			./config --prefix="$EXT_DIR" --openssldir="ssl_"
		fi
	fi
	
	make && make install_sw
	
	cd ..
	
	echo "Copy test certificates to ssl_ dir..."
	
	cp $DIR_NAME/demos/tunala/*.pem ssl_/certs/.
	
	echo
	echo "Done $WHAT!"
	echo
}


buildLeveldb()
{
	WHAT="leveldb"
	ADDR_NAME=$LEVELDB_ADDR
	echo
	echo "Building $WHAT..."
	echo

	OLD_DIR=`ls . | grep "$WHAT" | grep -v "tar"`
	echo
	echo "Cleanup previous builds..."
	echo

	rm -rf $OLD_DIR
	rm -rf include/leveldb
	rm -rf lib/libleveldb*
	
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
	export CXXFLAGS="-I$EXT_DIR/include -L$EXT_DIR/lib"
	make
	cp -r include/leveldb $EXT_DIR/include
	cp *.a $EXT_DIR/lib
	cd ..
	
	echo
	echo "Done $WHAT!"
	echo
}


function buildSnappy()
{
    WHAT="snappy"
	ADDR_NAME=$SNAPPY_ADDR
	echo
	echo "Building $WHAT..."
	echo

	OLD_DIR=`ls . | grep "$WHAT" | grep -v "tar"`
	echo
	echo "Cleanup previous builds..."
	echo

	rm -rf $OLD_DIR
	rm -rf include/snappy
	rm -rf lib/libsnappy*
	
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

	if [ -n "$DEBUG" ] ; then
		./configure --prefix="$EXT_DIR"
	else
		./configure --prefix="$EXT_DIR"
	fi
	make && make install
	cd ..
	echo
	echo "Done $WHAT!"
	echo
}

EXT_DIR="`pwd`"

BUILD_BOOST_MIN=
BUILD_BOOST_FULL=
BUILD_OPENSSL=
BUILD_LEVELDB=
BUILD_SNAPPY=

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
		BUILD_BOOST_FULL="yes"
		BUILD_OPENSSL="yes"
		BUILD_SOMETHING="yes"
		;;
	--boost)
		BUILD_BOOST_FULL="yes"
		BUILD_SOMETHING="yes"
		;;
	--openssl)
		BUILD_OPENSSL="yes"
		BUILD_SOMETHING="yes"
		;;
	--debug)
		DEBUG="yes"
		;;
	--force-download)
		DOWNLOAD="yes"
		;;
	--system)
		shift
		SYSTEM="$1"
		;;
	-h|--help)
		HELP="yes"
		BUILD_SOMETHING="yes"
		;;
	*)
		HELP="yes"
		;;
	esac
	shift
done

#echo "Debug build = $DEBUG"
#echo "Force download = $DOWNLOAD"

if [ "$HELP" = "yes" ]; then
	printUsage
	exit
fi

if [[ -z "${SYSTEM}" ]]; then
	SYSTEM=$(uname)
fi


echo "Extern folder: $EXT_DIR"
echo "System: $SYSTEM"

if [[ -z "${BUILD_SOMETHING}" ]]; then
	BUILD_BOOST_FULL="yes"
	BUILD_OPENSSL="yes"
fi

if [ $BUILD_OPENSSL ]; then
	buildOpenssl
fi

if [ $BUILD_BOOST_FULL ]; then
	buildBoost
else
	if [ $BUILD_BOOST_MIN ]; then
		buildBoost
	fi
fi


if [ $BUILD_SNAPPY ]; then
	buildSnappy
fi

if [ $BUILD_LEVELDB ]; then
	buildLeveldb
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
