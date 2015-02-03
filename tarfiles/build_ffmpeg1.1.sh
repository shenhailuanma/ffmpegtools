#!/bin/bash
set -e

current_dir=$(cd ../; pwd -P)
build_dir="${current_dir}/_build"
release_dir="${current_dir}/_release"
echo "start to build the tools for transcode system(current_dir: ${current_dir}, build_dir: ${build_dir}, release_dir: ${release_dir})..."

mkdir -p ${build_dir}
mkdir -p ${release_dir}
cp -rf libaacplus-2.0.2.tar.gz 26410-800.zip faac-1.28.tar.bz2 lame-3.98.4.tar.gz opencore-amr-0.1.2.tar.gz x264-snapshot-20140803-2245.tar.bz2 ffmpeg-1.1.tar.bz2 ${build_dir}
#cp -rf ffmpeg1.1_modify ${build_dir}


export PKG_CONFIG_PATH=${PKG_CONFIG_PATH}:${release_dir}/lib/pkgconfig

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
if ! [ -e "ffmpeg1.1" ]
then
    echo "########## ffmepg1.1 begin ##########"
    set -x
    tar xf ffmpeg-1.1.tar.bz2
    echo "remove all so to force the ffmpeg to build in static"
    rm -f ${release_dir}/lib/*.so*

    # patch ffmpeg
    pushd ${current_dir}
    cp -rf ffmpeg/ffmpeg1.1_modify/ffmpeg.c ${build_dir}/ffmpeg-1.1/
    cp -rf ffmpeg/ffmpeg1.1_modify/flvdec.c ${build_dir}/ffmpeg-1.1/libavformat/
    popd    

    pushd ffmpeg-1.1 

    # export the dir to enable the build command canbe use.  --disable-pthreads --extra-libs=-lpthread --enable-gpl
    export ffmpeg_exported_release_dir=${release_dir}
    echo ${ffmpeg_exported_release_dir}/include
    echo ${ffmpeg_exported_release_dir}/lib
./configure --prefix=${release_dir} --cc=$CC \
--extra-cflags="-I${ffmpeg_exported_release_dir}/include" --extra-ldflags="-L${ffmpeg_exported_release_dir}/lib -lm" \
--disable-pthreads --extra-libs=-lpthread --enable-gpl \
--enable-libmp3lame --enable-libx264 --enable-libfaac --enable-libaacplus \
--enable-postproc \
--enable-demuxer=oss \
--enable-static --enable-nonfree \
--enable-version3 --enable-libopencore-amrnb \

    echo "ffmpeg1.1 begin make"
    make
    #make install
    cp ffmpeg ${ffmpeg_exported_release_dir}/bin/ffmpeg1.1
    popd
    touch ffmpeg1.1
    echo "########## ffmpeg1.1 ok ##########"
else
    echo "########## ffmpeg1.1 has been installed ##########"
fi
popd


