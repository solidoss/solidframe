#!/bin/bash
if [ "$1" = "" ] ; then
	echo "Usage:"
	echo -ne "\n./build.sh [kdevelop] build_type\n\n"
	echo -ne "Where build type can be:\n"
	echo -ne "\tdebug - build uses full debug infos (-g3) and the debug log is activated\n"
	echo -ne "\tmaintain - same as debug but with compilation warnings activated\n"
	echo -ne "\tnolog - full debug info but logs are deactivated\n"
	echo -ne "\trelease - full optimization (-O3)\n"
	echo -ne "\tdocumentation_full - full API documentation including pdf\n"
	echo -ne "\tdocumentation_fast - fast API documentation\n"
	echo -ne "\nWhen used kdevelop, a kdevelop project will be created else make based project will be created\n\n"
	exit
fi

if [ "$1" = "kdevelop" ] ; then
	if [ "$2" = "" ] ; then
		echo "Usage:"
		echo "./build.sh [kdevelop] build_type"
		exit
	else
		mkdir application
		mkdir build
		mkdir "build/$1"
		cd "build/$1"
		cmake -G KDevelop3 -DCMAKE_BUILD_TYPE=$2 ../../
	fi
else 
	if [ "$1" = "documentation_full" ] ; then
		doxygen
	else 
		if [ "$1" = "documentation_fast" ] ; then
			doxygen Doxyfile.fast
		else
			mkdir application
			mkdir build
			mkdir "build/$1"
			cd "build/$1"
			cmake -DCMAKE_BUILD_TYPE=$1 ../../
		fi
	fi
fi
