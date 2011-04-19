#!/bin/bash
set -e
OUT=$PWD/out
cd encfs
if true; then
	make distclean || true
	autoreconf
	export lt_cv_deplibs_check_method='pass_all'
	#LDFLAGS="-L$OUT/lib -pg" CXXFLAGS="-pg -fprofile" CFLAGS="-pg -fprofile" CPPFLAGS="-I$OUT/include" ./configure --prefix=$OUT --with-boost=$OUT --without-libiconv-prefix --without-libintl-prefix --host=i586-mingw32msvc
	LDFLAGS=-L$OUT/lib CPPFLAGS=-I$OUT/include ./configure --prefix=$OUT --with-boost=$OUT --without-libiconv-prefix --without-libintl-prefix --host=i586-mingw32msvc
fi
cd encfs
rm -f encfs.exe encfsctl.exe libencfsall.la
make encfs.exe encfsctl.exe
test -r .libs/libencfsall-1.dll
DLL=$(i586-mingw32msvc-objdump -p .libs/libencfsall-1.dll | grep '^Name ' | sed 's/.*encfs/encfs/')
cp .libs/encfs.exe .libs/encfsctl.exe $OUT/bin
cp .libs/libencfsall-1.dll $OUT/bin/$DLL
cd ../..
echo encfs ok

