#!/bin/bash
if [ "$1" = "" ] ; then
	echo "Usage:"
	echo -ne "\n./bash.sh [kdevelop] build_type\n\n"
	echo -ne "Where build type can be:\n"
	echo -ne "\tdebug - build uses full debug infos (-g3) and the debug log is activated, also compilation warnings are on\n"
	echo -ne "\tnolog - full debug info but logs are deactivated\n"
	echo -ne "\trelease - full optimization (-O3)\n"
	echo -ne "\nWhen used kdevelop, a kdevelop project will be created else make based project will be created\n\n"
	exit
fi
mkdir application
mkdir build
mkdir "build/$1"
cd "build/$1"

if [ "$1" = "kdevelop" ] ; then
	if [ "$2" = "" ] ; then
		echo "Usage:"
		echo "./bash.sh [kdevelop] build_type"
		exit
	else
		cmake -G KDevelop3 -DCMAKE_BUILD_TYPE=$2 ../../
	fi
else
	cmake -DCMAKE_BUILD_TYPE=$1 ../../
fi
