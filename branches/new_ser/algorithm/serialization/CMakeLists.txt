add_library(serialization 
	src/binary.cpp src/typemapper.cpp
	
	#header files for visual studio projects
	basetypemap.hpp binary.hpp idtypemap.hpp nametypemap.hpp typemapper.hpp
)
