#include <stdio.h>

#include "lsp.h"


int main(int argc, char ** argv)
{
    printf("hello lsp\n");

    int ret ;
    struct Lsp_args args;
    struct Lsp_obj  obj;

    // init the args
    memset(&args, 0, sizeof(struct Lsp_args));
    args.have_video_stream = 1;
    args.have_audio_stream = 1;
    args.have_file_stream = 1;
    args.have_rtmp_stream = 1;

    strcpy(&args.stream_src_path, "/root/video/Meerkats.mp4")
    strcpy(&args.stream_out_path, "/root/video/out.flv")

    ret = Lsp_init(&obj, &args);
    printf("Lsp_init return:%d\n", ret);


    return 0;
}