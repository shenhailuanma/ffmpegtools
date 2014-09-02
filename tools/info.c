
#include <stdio.h>
#include <string.h>

#include "libavformat/avformat.h"

/*
int get_info_by_param(char *src, char *param)
{


    int ret;
    int val;
    int duration = 0;

    AVFormatContext *ctx = NULL;

    if(src == NULL || param == NULL)
        return -1;

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


    //av_dump_format(ctx, 0, ctx->filename, 0);
    printf("filename:%s\n", ctx->filename);
    printf("bitrate:%d\n", ctx->bit_rate);

    if(strcmp(param, "duration") == 0){
        duration = ctx->duration/AV_TIME_BASE;
        printf("get duration:%d\n",duration);
        return duration;
    }else{
        return -1;
    }


    avformat_close_input(&ctx);

    
    return 0;
}
*/

int get_duration(char *src)
{


    int ret;
    int val;
    int duration = 0;

    AVFormatContext *ctx = NULL;

    if(src == NULL)
        return -1;

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


    //av_dump_format(ctx, 0, ctx->filename, 0);
    printf("filename:%s\n", ctx->filename);
    printf("bitrate:%d\n", ctx->bit_rate);

    if(ctx->duration != AV_NOPTS_VALUE){
        duration = ctx->duration/AV_TIME_BASE;
        printf("get duration:%d\n",duration);
        return duration;
    }else{
        return -1;
    }


    avformat_close_input(&ctx);

    
    return 0;
}

int main(int argc, char ** argv)
{
    int ret = 0;

    ret = get_duration(argv[1]);
    printf("get duration:%d.\n",ret);

    return 0;
}
