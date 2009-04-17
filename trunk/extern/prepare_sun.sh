#!/bin/bash

BOOST_ARCH=`find . -name "boost_*.tar.bz2"`
OPENSSL_ARCH=`find . -name "openssl-*.tar.gz"`
TCLAP_ARCH=`find . -name "tclap-*.tar.gz"`

echo "Extract boost $BOOST_ARCH"
tar -xjf "$BOOST_ARCH"
echo "Extract openssl $OPENSSL_ARCH"
tar -xzf "$OPENSSL_ARCH"
echo "Extract  tclap $TCLAP_ARCH"
tar -xzf "$TCLAP_ARCH"

echo "Create arch folder and move the archives in it"
mkdir arch
mv *.bz2 arch
mv *.gz arch
mkdir include
cd include
cd ..
mkdir lib

#clean
rm -rf lib/*
rm -rf include/*

cd lib
cd ..

BOOST_DIR=`ls . | grep "boost"`
OPENSSL_DIR=`ls . | grep "openssl"`
TCLAP_DIR=`ls . | grep "tclap"`

EXT_DIR="`pwd`"
echo "Make boost..."
cd "$BOOST_DIR"
echo "$EXT_DIR"
./configure --with-toolset=sun --with-libraries="filesystem,system" --prefix="$EXT_DIR" --exec-prefix="$EXT_DIR"
#make && make install
tools/jam/src/bin.solaris/bjam toolset=sun stdlib=sun-stlport instruction-set=i586 address-model=32 --with-filesystem --with-system --prefix="$EXT_DIR" --exec-prefix="$EXT_DIR" install
#make install
cd ..
cd include
BOOST_INC_DIR=`ls . | grep "boost"`
ln -s "$BOOST_INC_DIR/boost" .
cd ../lib
LIB_LIST=`find . -name "libboost_*-mt-*.a"`
LIB_NAME=`find . -name "libboost_filesystem*-mt-*.a"`
ln -s $LIB_NAME libboost_filesystem.a
LIB_NAME=`find . -name "libboost_system*-mt-*.a"`
ln -s $LIB_NAME libboost_system.a
cd ../
#exit
echo "Make openssl..."
cd $OPENSSL_DIR
./Configure solaris-x86-cc  --prefix="$EXT_DIR" --openssldir="openssl_"
make
make install
cd ..
echo "Make tclap ..."
echo $TCLAP_DIR
cd include
ln -s "../$TCLAP_DIR/include/tclap" tclap

