
#include <stdio.h>
#include <string.h>

#include "libavformat/avformat.h"

static void print_help(void)
{
    printf("Usage: ./xspeed  -i <input_file> -s <speed_number> -o <output_file>\n");
    printf("\tinput_file: the input file.\n");
    printf("\tspeed_number: the number of speed change.\n");
    printf("\toutput_file: the output file.\n");
    printf("For example:\n");
    printf("\t./xspeed -i /data/video/source.mp4 -s 2 -o /tmp/out.mp4\n");
}


int get_info_by_param(char *src, char *param)
{


    int ret;
    int val;
    
    AVFormatContext *ctx = NULL;

    av_register_all();

    // open the media file
    ret = avformat_open_input(&ctx, src, NULL, NULL);
    if (ret < 0) {
        printf("[error] error open stream: '%s', error code: %d \n", src, ret);
        return -1;
    }

    ret = avformat_find_stream_info(ctx, NULL);
    if (ret < 0) {
        printf("[error] could not find stream info.\n");
        return -1;
    }

    av_dump_format(ctx, 0, ctx->filename, 0);

    if(strcpy(param, "duration") == 0){
        printf("get duration:%d", ctx->duration);
        return ctx->duration;
    }else{
        return -1;
    }


    avformat_close_input(&ctx);

    
    return 0;
}


int main(void)
{
    int ret = 0;

    ret = get_info_by_param("/tmp/out1.mp4","duration");
    printf("get duration:%d.",ret);

    return 0;
}
