FROM ubuntu:18.04 as windows-builder

RUN apt-get update && apt-get install -y gcc-multilib upx \
    mingw-w64 make build-essential git \
    && mkdir -p /src

RUN cd /src && \
    git clone https://github.com/jprjr/snes_spc.git && \
    cd snes_spc && \
    git checkout a7e3fb8d5eb0802d0f853556ac304a6edd8d6856 && \
    make AR=i686-w64-mingw32-ar CXX=i686-w64-mingw32-g++ PREFIX=/usr/i686-w64-mingw32 DYNLIB_EXT=.dll install && \
    make DYNLIB_EXT=.dll clean && \
    make AR=x86_64-w64-mingw32-ar CXX=x86_64-w64-mingw32-g++ PREFIX=/usr/x86_64-w64-mingw32 DYNLIB_EXT=.dll install && \
    make DYLNIB_EXT=.dll clean && \
    rm -f /usr/i686-w64-mingw32/lib/libspc.dll && \
    rm -f /usr/x86_64-w64-mingw32/lib/libspc.dll

RUN cd /src && \
    git clone https://github.com/jprjr/libid666.git && \
    cd libid666 && \
    make AR=i686-w64-mingw32-ar CC=i686-w64-mingw32-gcc  PREFIX=/usr/i686-w64-mingw32 DYNLIB_EXT=.dll install && \
    make DYNLIB_EXT=.dll clean && \
    make AR=x86_64-w64-mingw32-ar CC=x86_64-w64-mingw32-gcc  PREFIX=/usr/x86_64-w64-mingw32 DYNLIB_EXT=.dll install && \
    make DYLNIB_EXT=.dll clean && \
    rm -f /usr/i686-w64-mingw32/lib/libid666.dll && \
    rm -f /usr/x86_64-w64-mingw32/lib/libid666.dll

COPY . /src/spc2wav

RUN \
    cd /src/spc2wav && \
    mkdir -p /dist/win32 && \
    mkdir -p /dist/win64 && \
    make CC=i686-w64-mingw32-gcc EXE=.exe LDFLAGS="-s -mconsole" && \
    cp spc2wav.exe /dist/win32 && \
    cp spc2wav-wrapper.bat /dist/win32 && \
    make clean && \
    make CC=x86_64-w64-mingw32-gcc EXE=.exe LDFLAGS="-s -mconsole" && \
    cp spc2wav.exe /dist/win64 && \
    cp spc2wav-wrapper.bat /dist/win64 && \
    make clean

FROM alpine:3.10 as linux-builder
RUN apk add make gcc g++ musl-dev linux-headers && \
    mkdir -p /src

COPY --from=0 /src /src

RUN cd /src/snes_spc && \
    make install && \
    make clean

RUN cd /src/libid666 && \
    make install && \
    make clean

RUN \
    mkdir /dist && \
    cd /src/spc2wav && \
    make LDFLAGS="-s -static" && \
    cp -v spc2wav /dist/ && \
    make clean

FROM multiarch/crossbuild as osx-builder
ENV CROSS_TRIPLE=x86_64-apple-darwin14

COPY --from=0 /src /src

RUN cd /src/snes_spc && \
    make CXX=/usr/osxcross/bin/o64-clang++ AR=/usr/osxcross/bin/${CROSS_TRIPLE}-ar libspc.a && \
    /usr/osxcross/bin/${CROSS_TRIPLE}-ranlib libspc.a

RUN cd /src/libid666 && \
    make CC=/usr/osxcross/bin/o64-clang AR=/usr/osxcross/bin/${CROSS_TRIPLE}-ar libid666.a && \
    /usr/osxcross/bin/${CROSS_TRIPLE}-ranlib libid666.a && \
    ln -s /src/libid666 /src/id666

RUN \
    mkdir /dist && \
    cd /src/spc2wav && \
    make CC=/usr/osxcross/bin/o64-clang CFLAGS="-I/src/snes_spc -I/src -O2" LDFLAGS="-L/src/snes_spc -L/src/libid666 -lspc -lid666" && \
    cp spc2wav /dist && \
    make clean

FROM alpine:3.10
RUN apk add upx rsync tar zip gzip dos2unix && \
    mkdir -p /dist/linux && \
    mkdir -p /dist/osx && \
    mkdir -p /dist/win32 && \
    mkdir -p /dist/win64

COPY --from=windows-builder /dist/win32/* /dist/win32/
COPY --from=windows-builder /dist/win64/* /dist/win64/
COPY --from=osx-builder /dist/spc2wav /dist/osx/
COPY --from=linux-builder /dist/spc2wav /dist/linux/

RUN cd dist && \
    cd win32 && \
    unix2dos *.bat && \
    zip ../spc2wav-win32.zip *.exe *.bat && \
    cd ../win64 && \
    unix2dos *.bat && \
    zip ../spc2wav-win64.zip *.exe *.bat && \
    cd ../osx && \
    tar cvzf ../spc2wav-osx.tar.gz * && \
    cd ../linux && \
    tar cvzf ../spc2wav-linux.tar.gz * && \
    cd .. && \
    rm -rf win32 win64 linux osx

COPY entrypoint.sh /
ENTRYPOINT ["/entrypoint.sh"]

