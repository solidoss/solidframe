#! /bin/sh

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
	#rm -rf $BOOST_DIR
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
	#cd tools/jam
	#cd tools/build/v2/engine/
	bash bootstrap.sh --with-toolset=clang
	VARIANT_BUILD=
	
	if [ $DEBUG ] ; then
		VARIANT_BUILD="variant=debug"
	else
		VARIANT_BUILD="variant=release"
	fi
	
	if [ $BUILD_BOOST_FULL ] ; then
		./b2 toolset=clang --layout=system  --prefix="$EXT_DIR" --exec-prefix="$EXT_DIR" link=static threading=multi $VARIANT_BUILD install
	else
		./b2 toolset=clang --with-filesystem --with-system --with-program_options --with-test --with-thread --layout=system  --prefix="$EXT_DIR" --exec-prefix="$EXT_DIR" link=static threading=multi $VARIANT_BUILD  install
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
	if [ $DEBUG ] ; then
		CC=cc ./config --prefix="$EXT_DIR" --openssldir="ssl_"
	else
		CC=cc ./config --prefix="$EXT_DIR" --openssldir="ssl_"
	fi
	make && make install_sw
	cd ..
	echo "Copy test certificates to ssl_ dir..."
	cp $DIR_NAME/demos/tunala/*.pem ssl_/certs/.
	echo
	echo "Done $WHAT!"
	echo
}


EXT_DIR="`pwd`"

BUILD_BOOST_MIN=
BUILD_BOOST_FULL=
BUILD_OPENSSL=

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
	-h|--help)
		HELP="yes"
		BUILD_SOMETHING="yes"
		;;
	*)
		#HELP="yes"
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

echo "Extern folder: $EXT_DIR"

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
