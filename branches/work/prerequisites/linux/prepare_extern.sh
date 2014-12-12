#! /bin/sh

printUsage()
{
	echo "Usage:"
	echo "./prepare_extern.sh [-a|--all] [-w|--with LIB] [--force-down] [-d|--debug] [-z|--archive]"
	echo "Where LIB can be: boost|openssl"
	echo "Examples:"
	echo "./prepare_extern.sh -a"
	echo "./prepare_extern.sh -w boost"
	echo "./prepare_extern.sh -w boost --force-down -d"
	echo "./prepare_extern.sh -w boost -w openssl"
	echo
}

BOOST_ADDR="http://garr.dl.sourceforge.net/project/boost/boost/1.57.0/boost_1_57_0.tar.bz2"
OPENSSL_ADDR="http://www.openssl.org/source/openssl-1.0.1j.tar.gz"
LEVELDB_ADDR="https://leveldb.googlecode.com/files/leveldb-1.15.0.tar.gz"
SNAPPY_ADDR="http://snappy.googlecode.com/files/snappy-1.0.5.tar.gz"

downloadArchive()
{
	echo "Downloading: [$1]"
	#wget $1
	curl -O $1
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
	sh bootstrap.sh
	
	if [ $DEBUG ] ; then
		VARIANT_BUILD="variant=debug"
	else
		VARIANT_BUILD="variant=release"
	fi
	
	if [ $BUILD_BOOST_FULL ] ; then
		./b2 --layout=system  --prefix="$EXT_DIR" --exec-prefix="$EXT_DIR" link=static threading=multi $VARIANT_BUILD install
	else
		./b2 --with-filesystem --with-system --with-program_options --with-test --with-thread --layout=system  --prefix="$EXT_DIR" --exec-prefix="$EXT_DIR" link=static threading=multi $VARIANT_BUILD  install
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
		./config --prefix="$EXT_DIR" --openssldir="ssl_"
	else
		./config --prefix="$EXT_DIR" --openssldir="ssl_"
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
echo "$EXT_DIR"

BUILD_BOOST_MIN=
BUILD_BOOST_FULL=
BUILD_OPENSSL=
BUILD_LEVELDB=
BUILD_SNAPPY=

ARCHIVE=
DOWNLOAD=
DEBUG=
HELP="yes"


while [ "$#" -gt 0 ]; do
	CURRENT_OPT="$1"
	UNKNOWN_ARG=no
	HELP=
	case "$1" in
	-a|--all)
		BUILD_BOOST_FULL="yes"
		BUILD_OPENSSL="yes"
		;;
	-w|--with)
		shift
		#BUILD_LIBS=$BUILD_LIBS:"$1"
		if [ "$1" = "boost" ] ; then
			BUILD_BOOST_FULL="yes"
		fi
		if [ "$1" = "openssl" ] ; then
			BUILD_OPENSSL="yes"
		fi
		if [ "$1" = "snappy" ] ; then
			BUILD_SNAPPY="yes"
		fi
		if [ "$1" = "leveldb" ] ; then
			BUILD_LEVELDB="yes"
		fi
		;;
	-d|--debug)
		shift
		DEBUG="yes"
		;;
	-z|--archive)
		shift
		ARCHIVE="yes"
		;;
	--force-down)
		DOWNLOAD="yes"
		;;
	-h|--help)
		HELP="yes"
		;;
	*)
		HELP="yes"
		;;
	esac
	shift
done

echo "Debug build = $DEBUG"
echo "Force download = $DOWNLOAD"

if [ "$HELP" = "yes" ]; then
	printUsage
	exit
fi

if [ $BUILD_BOOST_FULL ]; then
	buildBoost
else
	if [ $BUILD_BOOST_MIN ]; then
		buildBoost
	fi
fi

if [ $BUILD_OPENSSL ]; then
	buildOpenssl
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
