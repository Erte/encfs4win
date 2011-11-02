#!/bin/bash
set -e
NAME=openssl-1.0.0e
OUT=$PWD/../out
rm -rf $NAME
tar zxvf $NAME.tar.gz
cd $NAME
CC=i586-mingw32msvc-gcc ./Configure --prefix=$OUT mingw
make CC=i586-mingw32msvc-gcc RANLIB=i585-mingw32msvc-ranlib || true
i586-mingw32msvc-ranlib libssl.a 
i586-mingw32msvc-ranlib libcrypto.a 
make CC=i586-mingw32msvc-gcc RANLIB=i585-mingw32msvc-ranlib
make install
cd ..
rm -rf $NAME
echo OpenSSL ok

