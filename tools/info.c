
#include <stdio.h>
#include <string.h>

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"


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

/*
    Function: zongshijian
    Input:  
        src : source file path
    Return:
        > 0 : return the duration time. unit is second. e.g.  if return value is 12, means the src file duration is 12s.
        not 0 : means error
*/
int zongshijian(char *src)
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
    if (ret < 0 || ctx == NULL) {
        printf("[error] error open stream: '%s', error code: %d \n", src, ret);
        return -1;
    }

    ret = avformat_find_stream_info(ctx, NULL);
    if (ret < 0) {
        printf("[error] could not find stream info.\n");
        return -1;
    }


    //av_dump_format(ctx, 0, ctx->filename, 0);
    //printf("filename:%s\n", ctx->filename);
    //printf("bitrate:%d\n", ctx->bit_rate);

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

/*
    Function: jietu
    Input:  
        src : source file path
        jietime: the time of the picture which you want to get. unit is ms, e.g. 4000 means 4 s.
        dest_path: the dest file path

    Return:
        0 : means ok
        not 0 : means error
*/
int jietu(char * src, int jietime, char * dest_path)
{
    int ret;
    int i;

    AVFormatContext * inctx = NULL;
    int input_video_stream_index = -1;
    AVCodecContext  * inVideoCodecCtx = NULL;
    int inWidth  = 0;
    int inHeight = 0;
    AVCodec * inViedeCodec = NULL;

    AVFormatContext * outctx = NULL;
    AVOutputFormat  * fmt = NULL;  // output format
    AVStream        * video_st = NULL;    // output stream
    AVCodecContext  * outVideoCodecCtx = NULL;
    AVCodec * outViedeCodec = NULL;

    AVPacket pkt; 

    if(src == NULL || dest_path == NULL){
        printf("[error]: jietu input params NULL.\n");
        return -1;
    }

    if(jietime <= 0){
        printf("[warning] jietime <= 0, modify it to 0.\n");
        jietime = 0;
    }

    av_register_all();

    //// open the input file
    ret = avformat_open_input(&inctx, src, NULL, NULL);
    if (ret < 0 || inctx == NULL) {
        printf("[error] error open stream: '%s', error code: %d \n", src, ret);
        return -1;
    }

    ret = avformat_find_stream_info(inctx, NULL);
    if (ret < 0) {
        printf("[error] could not find stream info.\n");
        return -1;
    }

    inctx->flags |= AVFMT_FLAG_GENPTS;
    av_dump_format(inctx, 0, inctx->filename, 0);

    // find the video stream
    input_video_stream_index = -1;
    for (i = 0; i < inctx->nb_streams; i++) {
        if (input_video_stream_index == -1 && inctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            input_video_stream_index = i;
            inWidth  = inctx->streams[i]->codec->width;
            inHeight = inctx->streams[i]->codec->height;
            break;
        }
    }

    if(input_video_stream_index == -1){
        printf("[error] not found video stream.\n");
        return -1;
    }

    // open input video decodec
    inViedeCodec = avcodec_find_decoder(inctx->streams[input_video_stream_index]->codec->codec_id);
    if(inViedeCodec == NULL){
        printf("[error] inViedeCodec %d not found !\n", inctx->streams[input_video_stream_index]->codec->codec_id);
        return -1;
    }else{
        // open it 
        ret = avcodec_open2(inctx->streams[input_video_stream_index]->codec, inViedeCodec, NULL);
        if (ret < 0) {
            printf("[error] could not open codec\n");
            return -1;
        }
    }
    //// open the output file
    outctx = avformat_alloc_context();
    fmt = av_guess_format("mjpeg", NULL, NULL);
    if(fmt == NULL){
        printf("[error] guess format mjepg error.\n");
        return -1;
    }
    outctx->oformat = fmt;  

    if (avio_open(&outctx->pb, dest_path, AVIO_FLAG_READ_WRITE) < 0){
        printf("[error] open output file.\n");
        return -1;
    }

    video_st = av_new_stream(outctx, outctx->nb_streams);
    if (video_st==NULL){
        printf("[error] av_new_stream error.\n");
        return -1;
    }

    avcodec_get_context_defaults3(video_st->codec, NULL);
    outVideoCodecCtx = video_st->codec;
    outVideoCodecCtx->codec_id = fmt->video_codec;
    outVideoCodecCtx->codec_type = AVMEDIA_TYPE_VIDEO;
    outVideoCodecCtx->pix_fmt = PIX_FMT_YUVJ420P;

    outVideoCodecCtx->width  = inWidth;  // out width same as input width
    outVideoCodecCtx->height = inHeight; // out height same as input height

    outVideoCodecCtx->time_base.num = 1;  
    outVideoCodecCtx->time_base.den = 25;   

    av_dump_format(outctx, 0, dest_path, 1);  

    // open the output encoder
    outViedeCodec = avcodec_find_encoder(outVideoCodecCtx->codec_id);
    if(outViedeCodec == NULL){
        printf("[error] outViedeCodec %d not found !\n", outVideoCodecCtx->codec_id);
        return -1;
    }else{
        // open it 
        ret = avcodec_open2(outVideoCodecCtx, outViedeCodec, NULL);
        if (ret < 0) {
            printf("[error] could not open codec\n");
            return -1;
        }
    }

    // seek the jietu time, AV_TIME_BASE=1000000 
    ret = av_seek_frame(inctx, -1, jietime*1000, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        printf("[error] could not seek to position %d\n", jietime);
        return -1;
    }

    // get the frame
    while(1){
        av_init_packet(&pkt);
        ret = av_read_frame(inctx, &pkt);
        if(ret == AVERROR(EAGAIN)){
            // EAGAIN means try again
            printf("[warning] av_read_frame ret=%d, continue.\n", ret);
            continue;
        }
        if(ret < 0){
            printf("[error] av_read_frame error, ret=%d.\n",ret);
            return -1;
        }

        if(pkt.stream_index == input_video_stream_index){
            printf("pkt dts: %lld ,pts: %lld, id_key:%d \n", pkt.dts, pkt.pts, pkt.flags & PKT_FLAG_KEY);
            break;
        }
    }

    // decode the frame

    // encode the frame to jpg format

    return 0;
}


int main(int argc, char ** argv)
{
    int ret = 0;

    ret = zongshijian(argv[1]);
    printf("get duration:%d.\n",ret);

    ret = jietu(argv[1], 1000*3, "/tmp/jietu.jpg");
    printf("jietu:%d.\n",ret);

    return 0;
}
