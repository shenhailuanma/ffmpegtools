#!/bin/bash

echo ${PWD}

current_dir=$(cd ../; pwd -P)
build_dir="${current_dir}/_build"
release_dir="${current_dir}/_release"
echo "start to build the tools for transcode system(current_dir: ${current_dir}, build_dir: ${build_dir}, release_dir: ${release_dir})..."

mkdir -p ${build_dir}
mkdir -p ${release_dir}



ffmpeg_build_dir="${build_dir}/ffmpeg-2.2"
cp -rf slicer.c ${ffmpeg_build_dir}

gcc -o ${current_dir}/tools/slicer ${ffmpeg_build_dir}/slicer.c -I${ffmpeg_build_dir} -I${release_dir}/include \
${release_dir}/libavformat/libavformat.a \
${release_dir}/libavcodec/libavcodec.a \
${release_dir}/libavdevice/libavdevice.a \
${release_dir}/libavfilter/libavfilter.a \
${release_dir}/libavutil/libavutil.a \
${release_dir}/libswscale/libswscale.a \
${release_dir}/libswresample/libswresample.a \
${release_dir}/lib/libopencore-amrnb.a \
${release_dir}/lib/libopencore-amrwb.a \
${release_dir}/libpostproc/libpostproc.a \
${release_dir}/lib/libmp3lame.a \
${release_dir}/lib/libfaac.a \
${release_dir}/lib/libaacplus.a \
${release_dir}/lib/libx264.a -lm  -lbz2 -lz -lpthread