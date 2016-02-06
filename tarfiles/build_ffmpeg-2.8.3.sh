#!/bin/bash
set -e

current_dir=$(cd ../; pwd -P)
build_dir="${current_dir}/_build"
release_dir="${current_dir}/_release"
echo "start to build the tools for transcode system(current_dir: ${current_dir}, build_dir: ${build_dir}, release_dir: ${release_dir})..."

mkdir -p ${build_dir}
mkdir -p ${release_dir}
cp -rf yasm-1.2.0.tar.gz libaacplus-2.0.2.tar.gz 26410-800.zip faac-1.28.tar.bz2 lame-3.98.4.tar.gz opencore-amr-0.1.2.tar.gz x264-snapshot-20140803-2245.tar.bz2 ffmpeg-2.8.3.tar.gz ${build_dir}


export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:${release_dir}/lib/pkgconfig
export PATH=${PATH}:${release_dir}/bin

# yasm
pushd ${build_dir} 
if ! [ -e "yasm" ]
then
    echo "########## libx264 begin ##########"
    tar xf yasm-1.2.0.tar.gz
    pushd yasm-1.2.0
    ./configure 
    make
    make install
    popd
    touch yasm
    echo "########## yasm ok ##########"
else
    echo "########## yasm has been installed ##########"
fi
popd


# libaacplus
pushd ${build_dir}
if ! [ -e "aacplus" ]
then
    echo "########## libaacplus begin #########"
    tar xf libaacplus-2.0.2.tar.gz
    cp 26410-800.zip libaacplus-2.0.2/src
    pushd libaacplus-2.0.2
    sed -i "221s/&&/#\&\&/" ./src/Makefile.am
    bash autogen.sh
    ./configure --prefix=${release_dir} --enable-static
    make
    make install
    popd
    touch aacplus
    echo "######### libaacplus ok #########"
else
    echo "######### libaacplus has been installed #########" 
fi
popd



# libfaac
pushd ${build_dir}
if ! [ -e "faac" ]
then
    echo "########## libfaac begin ##########"
    tar xf faac-1.28.tar.bz2
    pushd faac-1.28
    ./configure --prefix=${release_dir} --enable-static --without-mp4v2
    make
    make install
    popd
    touch faac
    echo "########## libfaac ok ##########"
else
    echo "########## libfaac has been installed ##########"
fi
popd


# libmp3lame
pushd ${build_dir} 
if ! [ -e "mp3lame" ]
then
    echo "########## libmp3lame begin ##########"
    tar xf lame-3.98.4.tar.gz
    pushd lame-3.98.4
    ./configure --prefix=${release_dir} --enable-static
    make
    make install
    popd
    touch mp3lame
    echo "########## libmp3lame ok ##########"
else
    echo "########## libmp3lame has been installed ##########"
fi
popd

# libopencore_amrnb
pushd ${build_dir} 
if ! [ -e "opencore_amrnb" ]
then
    echo "########## libopencore_amrnb begin ##########"
    tar xf opencore-amr-0.1.2.tar.gz
    pushd opencore-amr-0.1.2
    ./configure --prefix=${release_dir} --enable-static
    make
    make install
    popd
    touch opencore_amrnb
    echo "########## libopencore_amrnb ok ##########"
else
    echo "########## libopencore_amrnb has been installed ##########"
fi
popd



# libx264
pushd ${build_dir} 
if ! [ -e "x264" ]
then
    echo "########## libx264 begin ##########"
    tar xf x264-snapshot-20140803-2245.tar.bz2
    pushd x264-snapshot-20140803-2245
    ./configure --prefix=${release_dir} --enable-static --disable-opencl
    sed -i -e 's/-s //' -e 's/-s$//' config.mak
    make
    make install
    popd
    touch x264
    echo "########## libx264 ok ##########"
else
    echo "########## libx264 has been installed ##########"
fi
popd





# ffmpeg
pushd ${build_dir} 
if ! [ -e "ffmpeg2.8.3" ]
then
    echo "########## ffmepg begin ##########"
    set -x
    tar xf ffmpeg-2.8.3.tar.gz
    echo "remove all so to force the ffmpeg to build in static"
    rm -f ${release_dir}/lib/*.so*

    # patch ffmpeg
    pushd ffmpeg-2.8.3 

    # export the dir to enable the build command canbe use.  --disable-pthreads --extra-libs=-lpthread --enable-gpl
    export ffmpeg_exported_release_dir=${release_dir}
    echo ${ffmpeg_exported_release_dir}/include
    echo ${ffmpeg_exported_release_dir}/lib
./configure --prefix=${release_dir} --cc=$CC \
    --extra-cflags="-I${ffmpeg_exported_release_dir}/include" --extra-ldflags="-L${ffmpeg_exported_release_dir}/lib -lm" \
    --disable-hwaccels \
    --disable-lzma \
    --disable-yasm \
    --disable-devices  \
--extra-libs=-lpthread --enable-gpl \
--enable-libx264 \
--enable-postproc \
--enable-static --enable-nonfree \
--enable-version3 --enable-libopencore-amrnb \
--enable-parser=h264


    echo "ffmpeg2.8.3 begin make"
    make
    make install
    popd
    touch ffmpeg2.8.3
    echo "########## ffmpeg2.8.3 ok ##########"
else
    echo "########## ffmpeg2.8.3 has been installed ##########"
fi
popd


