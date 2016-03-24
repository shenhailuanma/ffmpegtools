#include <stdio.h>

#include "lsp.h"


int main(int argc, char ** argv)
{
    printf("hello lsp\n");

    int ret ;
    struct Lsp_args args;
    Lsp_handle handle = Lsp_create();

    // init the args
    memset(&args, 0, sizeof(struct Lsp_args));

    // set the params which should must be set.
    args.have_video_stream = 1;
    args.have_audio_stream = 1;

    strcpy(&args.stream_src_path, "/root/video/Meerkats.mp4");
    strcpy(&args.stream_out_path, "/root/video/out.flv");
    args.i_channels = 2;
    args.i_sample_rate = 44100;
    args.i_sample_fmt = AUDIO_SAMPLE_FORMAT_TYPE_U8;

    ret = Lsp_init(handle, &args);
    printf("Lsp_init return:%d\n", ret);


    return 0;
}