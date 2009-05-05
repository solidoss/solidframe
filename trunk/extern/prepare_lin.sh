#!/bin/bash

BOOST_ARCH=`find . -name "boost_*.tar.bz2"`
OPENSSL_ARCH=`find . -name "openssl-*.tar.gz"`

echo "Extract boost $BOOST_ARCH"
tar -xjf "$BOOST_ARCH"
echo "Extract openssl $OPENSSL_ARCH"
tar -xzf "$OPENSSL_ARCH"

echo "Make the arch folder and move the archives there"
mkdir arch
mv *.bz2 arch
mv *.gz arch

echo "Make the include folder"
mkdir include
cd include

cd ..
echo "Make the lib folder"
mkdir lib

#clean
echo "Clean the lib and include folders"
rm -rf lib/*
rm -rf include/*

cd lib
cd ..

BOOST_DIR=`ls . | grep "boost"`
OPENSSL_DIR=`ls . | grep "openssl"`

EXT_DIR="`pwd`"

echo "Make boost..."
cd "$BOOST_DIR"
echo "$EXT_DIR"
#./configure --with-libraries="date_time,filesystem,regex,system,serialization,program_options" --prefix="$EXT_DIR" --exec-prefix="$EXT_DIR"
./configure --with-libraries="filesystem,system,program_options" --prefix="$EXT_DIR" --exec-prefix="$EXT_DIR"
make && make install
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
LIB_NAME=`find . -name "libboost_program_options*-mt-*.a"`
ln -s $LIB_NAME libboost_program_options.a

echo "Make openssl..."
cd ../$OPENSSL_DIR
./config --prefix="$EXT_DIR" --openssldir="openssl_"
make
make install
cd ..
echo "Copy test certificates to openssl_ dir..."
cp $OPENSSL_DIR/demos/tunala/*.pem openssl_/certs/.
echo
echo "DONE!!"
