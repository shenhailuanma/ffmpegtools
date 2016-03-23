

#!/bin/bash

set -e

echo ${PWD}


# params which need be modified.
PREFIX=${PWD}/_release_ffmpeg_1_1
FFMPEG_TAR=${PWD}/tarfiles/ffmpeg-1.1.tar.bz2

# the NDK path
NDK=/opt/android-ndk-r10
SYSROOT=$NDK/platforms/android-19/arch-arm/
TOOLCHAIN=$NDK/toolchains/arm-linux-androideabi-4.6/prebuilt/linux-x86_64

# build the ffmpeg libs

mkdir -p _build
pushd _build

tar xf ${FFMPEG_TAR}
pushd ffmpeg-1.1 

./configure  \
  --cross-prefix=$TOOLCHAIN/bin/arm-linux-androideabi- --target-os=linux  --arch=arm  --enable-cross-compile \
  --sysroot=$SYSROOT  --extra-cflags="-Os -fpic $ADDI_CFLAGS"  \
  --prefix=$PREFIX --disable-shared   --enable-static --disable-doc \
  --disable-hwaccels \
  --disable-debug \
  --enable-nonfree \
  --enable-version3 \
  --disable-yasm  \
  --disable-decoders \
  --disable-parsers \
  --disable-encoders \
  --enable-decoder=h264 \
  --enable-decoder=aac \
  --enable-decoder=ac3 \
  --enable-decoder=ape \
  --enable-decoder=mp3 \
  --enable-decoder=mpeg2video \
  --enable-decoder=pcm_s16be \
  --enable-decoder=pcm_s16le \
  --enable-decoder=pcm_s32be \
  --enable-decoder=pcm_s32le \
  --enable-decoder=flac \
  --enable-decoder=flv \
  --enable-decoder=zlib \
  --enable-decoder=h264 \
  --enable-decoder=flv \
  --enable-encoder=mjpeg


make clean

make

make install

popd

popd
