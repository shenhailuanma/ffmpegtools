
#!/bin/bash

set -e

echo ${PWD}

PREFIX=${PWD}/_release_ffmpeg_2_3_3
FFMPEG_TAR=${PWD}/tarfiles/ffmpeg-2.3.3.tar.bz2

function build_one  
{
mkdir -p _build
pushd _build

tar xf ${FFMPEG_TAR}
pushd ffmpeg-2.3.3 

./configure  --prefix=$PREFIX --disable-shared   --enable-static --disable-doc \
  --disable-hwaccels \
  --disable-debug \
  --enable-nonfree \
  --enable-version3 \
  --disable-yasm  
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
}

build_one
