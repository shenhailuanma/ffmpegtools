#!/bin/bash

echo ${PWD}

current_dir=$(cd ../; pwd -P)
build_dir="${current_dir}/_build"
release_dir="${current_dir}/_release"
echo "start to build the tools for transcode system(current_dir: ${current_dir}, build_dir: ${build_dir}, release_dir: ${release_dir})..."

mkdir -p ${build_dir}
mkdir -p ${release_dir}


gcc -o ${current_dir}/tools/slicer ${current_dir}/tools/slicer.c -I${release_dir}/include \
${release_dir}/lib/libavformat.a \
${release_dir}/lib/libavcodec.a \
${release_dir}/lib/libavdevice.a \
${release_dir}/lib/libavfilter.a \
${release_dir}/lib/libavutil.a \
${release_dir}/lib/libswscale.a \
${release_dir}/lib/libswresample.a \
${release_dir}/lib/libopencore-amrnb.a \
${release_dir}/lib/libopencore-amrwb.a \
${release_dir}/lib/libpostproc.a \
${release_dir}/lib/libmp3lame.a \
${release_dir}/lib/libfaac.a \
${release_dir}/lib/libaacplus.a \
${release_dir}/lib/libx264.a -lm  -lbz2 -lz -lpthread