#!/bin/bash

set -e

echo ${PWD}

current_dir=$(cd ../; pwd -P)
build_dir="${current_dir}/_build"
release_dir="${current_dir}/_release"
echo "current_dir: ${current_dir}, build_dir: ${build_dir}, release_dir: ${release_dir}"

mkdir -p ${build_dir}
mkdir -p ${release_dir}


export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:${release_dir}/lib/pkgconfig
export PATH=${PATH}:${release_dir}/bin

# yasm
pushd ${build_dir} 
if ! [ -e "yasm-ok" ]
then
    echo "########## yasm begin ##########"
    if ! [ -e "yasm-1.3.0.tar.gz" ]
    then
        wget http://www.tortall.net/projects/yasm/releases/yasm-1.3.0.tar.gz
    fi
    tar xf yasm-1.3.0.tar.gz
    pushd yasm-1.3.0
    ./configure 
    make
    make install
    popd
    touch yasm-ok
    echo "########## yasm ok ##########"
else
    echo "########## yasm has been installed ##########"
fi
popd


# libx264
pushd ${build_dir} 
if ! [ -e "x264-ok" ]
then
    echo "########## libx264 begin ##########"
    if ! [ -d "x264" ]
    then
        git clone --depth 1 git://git.videolan.org/x264
    fi
    pushd x264
    ./configure --prefix=${release_dir} --enable-static --disable-opencl
    make
    make install
    popd
    touch x264-ok
    echo "########## libx264 ok ##########"
else
    echo "########## libx264 has been installed ##########"
fi
popd

# get the ffmpeg 
pushd ${build_dir} 
if ! [ -e "ffmpeg-2.2.16-ok" ]
then
    echo "########## ffmpeg-2.2.16 begin ##########"
    # to get the ffmpeg
    if ! [ -e "ffmpeg-2.2.16.tar.gz" ]
    then
        echo "########## ffmpeg-2.2.16 downloading ##########"
        wget http://ffmpeg.org/releases/ffmpeg-2.2.16.tar.gz
    else
        echo "########## ffmpeg-2.2.16 has downloaded ##########"
    fi
    tar xf ffmpeg-2.2.16.tar.gz
    pushd ffmpeg-2.2.16
    export ffmpeg_exported_release_dir=${release_dir}
    echo ${ffmpeg_exported_release_dir}/include
    echo ${ffmpeg_exported_release_dir}/lib
    ./configure --prefix=${release_dir} --cc=$CC \
--extra-cflags="-I${ffmpeg_exported_release_dir}/include" --extra-ldflags="-L${ffmpeg_exported_release_dir}/lib -lm" \
--disable-pthreads --extra-libs=-lpthread --enable-gpl \
--enable-libx264  \
--enable-postproc \
--enable-demuxer=oss \
--enable-static --enable-nonfree \
--enable-version3 

    make
    make install
    popd
    touch ffmpeg-2.2.16-ok
    echo "########## ffmpeg-2.2.16 over ##########"

else
    echo "########## ffmpeg-2.2.16 has existed ##########"
fi
popd


ffmpeg_build_dir="${build_dir}/ffmpeg-2.2.16"
cp -rf slicer.c ${ffmpeg_build_dir}

gcc -o ${release_dir}/bin/slicer ${ffmpeg_build_dir}/slicer.c -I${ffmpeg_build_dir} -I${release_dir}/include \
${ffmpeg_build_dir}/libavformat/libavformat.a \
${ffmpeg_build_dir}/libavcodec/libavcodec.a \
${ffmpeg_build_dir}/libavdevice/libavdevice.a \
${ffmpeg_build_dir}/libavfilter/libavfilter.a \
${ffmpeg_build_dir}/libavutil/libavutil.a \
${ffmpeg_build_dir}/libswscale/libswscale.a \
${ffmpeg_build_dir}/libswresample/libswresample.a \
${ffmpeg_build_dir}/libpostproc/libpostproc.a \
${release_dir}/lib/libx264.a -lm -lz -lpthread -g