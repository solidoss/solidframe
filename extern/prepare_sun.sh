#!/bin/bash
############################################################

BOOST_ARCH=`find . -name "boost_*.tar.bz2"`
OPENSSL_ARCH=`find . -name "openssl-*.tar.gz"`

############################################################
echo
echo "Extracting archives ..."
echo
############################################################
echo "Extracting boost [$BOOST_ARCH]..."
tar -xjf "$BOOST_ARCH"
echo "Extracting openssl [$OPENSSL_ARCH]..."
tar -xzf "$OPENSSL_ARCH"

############################################################

BOOST_DIR=`ls . | grep "boost" | grep -v "tar"`
OPENSSL_DIR=`ls . | grep "openssl" | grep -v "tar"`

EXT_DIR="`pwd`"
echo "$EXT_DIR"

############################################################
echo
echo "Making boost[$BOOST_DIR]..."
echo
############################################################
cd "$BOOST_DIR"
cd tools/jam
sh build_dist.sh
cd ../../
JAMEXE=`find . -name bjam`
echo "Using jam: $JAMEXE"
$JAMEXE toolset=sun stdlib=sun-stlport instruction-set=i586 address-model=32 --with-filesystem --with-system --with-program_options --with-test --prefix="$EXT_DIR" --exec-prefix="$EXT_DIR" install
cd ../

############################################################
echo
echo "Making openssl[$OPENSSL_DIR]..."
echo
############################################################

cd $OPENSSL_DIR
./Configure solaris-x86-cc  --prefix="$EXT_DIR" --openssldir="openssl_"
make
make install
cd ..
echo "Copy test certificates to openssl_ dir..."
cp $OPENSSL_DIR/demos/tunala/*.pem openssl_/certs/.
echo
echo "DONE!!"

