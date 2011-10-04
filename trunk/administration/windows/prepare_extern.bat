@echo off
set PWD=%cd%
echo %PWD%
cd boost_1_47_0
call bootstrap.bat
call bjam.exe --without-mpi --without-python --prefix="%PWD%" --exec-prefix="%PWD%" link=static threading=multi variant=release --layout=system runtime-link=shared install
cd ..
cd openssl-1.0.0d
perl Configure VC-WIN32 no-asm --prefix="%PWD%"
call ms\do_ms.bat
nmake -f ms\nt.mak
nmake -f ms\nt.mak install


