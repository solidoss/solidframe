@echo off
set PWD=%cd%
echo %PWD%
::todo: automatically edit boost\config\user.hpp and set BOOST_ALL_NO_LIB
cd boost_1_48_0
call bootstrap.bat
call bjam.exe --without-mpi --without-python --prefix="%PWD%" --exec-prefix="%PWD%" link=static threading=multi variant=release --layout=system runtime-link=static install
cd ..
cd openssl-1.0.0g
perl Configure VC-WIN32 no-asm --prefix="%PWD%"
call ms\do_ms.bat
nmake -f ms\nt.mak
nmake -f ms\nt.mak install


