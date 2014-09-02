
#!/bin/bash

set -e

echo ${PWD}

PREFIX=${PWD}_release_ffmpeg_2_3_3
FFMPEG_TAR=ffmpeg-2.3.3.tar.bz2

function build_one  
{
tar xf ${FFMPEG_TAR}
pushd ffmpeg-2.3.3 

./configure  --prefix=$PREFIX --disable-shared   --enable-static --disable-doc

make clean

make

make install

popd
}

build_one
