#!/bin/bash

function make_cmake_list(){
	rm CMakeLists.txt
	if [ $1 = "*" ]; then
		echo "No application directory found!"
	else
		for name in $*
			do
			if [ $name = "CMakeLists.txt" ]; then
				echo "Skip CMakeLists.txt"
			else
				echo "Adding dir: " $name
				echo -ne "add_subdirectory("$name")\n" >> CMakeLists.txt
			fi
			done
	fi
}

if [ "$1" = "" ] ; then
	echo "Usage:"
	echo -ne "\n./build.sh [kdevelop] build_type\n\n"
	echo -ne "Where build type can be:\n"
	echo -ne "\tdebug - build uses full debug infos (-g3) and the debug log is activated\n"
	echo -ne "\tmaintain - same as debug but with compilation warnings activated\n"
	echo -ne "\tnolog - full debug info but logs are deactivated\n"
	echo -ne "\trelease - full optimization (-O3)\n"
	echo -me "\textern - build the tar.gz with the extern libs\n"
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
		cd application
		make_cmake_list *
		cd ../
		mkdir build
		mkdir "build/$1"
		cd "build/$1"
		cmake -G KDevelop3 -DCMAKE_BUILD_TYPE=$2 ../../
	fi
else 
	if [ "$1" = "documentation_full" ] ; then
		rm -rf documentation/html/
		rm -rf documentation/latex/
		doxygen
		tar -cjf documentation/full.tar.bz2 documentation/html/ documentation/index.html
	else 
		if [ "$1" = "documentation_fast" ] ; then
			rm -rf documentation/html/
			doxygen Doxyfile.fast
			tar -cjf documentation/fast.tar.bz2 documentation/html/ documentation/index.html
		else
			if [ "$1" = "extern" ] ; then
				tar -cjf extern/solidground_extern_linux.tar.bz2 extern/linux
			else
				mkdir application
				cd application
				make_cmake_list *
				cd ../
				mkdir build
				mkdir "build/$1"
				cd "build/$1"
				cmake -DCMAKE_BUILD_TYPE=$1 ../../
			fi
		fi
	fi
fi
