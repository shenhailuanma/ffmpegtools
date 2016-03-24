#include <stdio.h>

#include "lsp.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/common.h>


struct object{
    
    int video_frame_cnt;
    int audio_sample_cnt;
    
    char src_path[128];
    AVFormatContext *src_ctx;  // src ctx

    int video_stream_index;
    int audio_stream_index;

    int input_video_timebase_num;
    int input_video_timebase_den;
    int input_video_stream_timebase_num;
    int input_video_stream_timebase_den;

    int input_audio_stream_timebase_num;
    int input_audio_stream_timebase_den;
};





void msSleep(int const milliSecond)
{
   
    struct timespec sleepTime;
    struct timespec sleepTimeBuf;

    int ret = -1;
    sleepTime.tv_sec = (milliSecond / 1000);
    sleepTime.tv_nsec = (1000 * 1000 * (milliSecond % 1000));
    /*
    while((ret < 0) && ((sleepTime.tv_nsec > 0) || (sleepTime.tv_sec > 0)))
    {
        ret = nanosleep(&sleepTime, &sleepTime);
    }
    */
    sleepTimeBuf = sleepTime;
    while(1)
    {
        sleepTime = sleepTimeBuf;
        ret = nanosleep(&sleepTime, &sleepTimeBuf);
        if((ret < 0)&&(errno == EINTR))
            continue;
        else
            break;
    }
    
}

static int open_input_media(struct object * obj)
{
    if(obj == NULL){
        printf("open_input_media obj is NULL\n");
        return -1;
    }

    int ret = 0;
    int i;

    obj->src_ctx = NULL;
    ret = avformat_open_input(&obj->src_ctx, obj->src_path, NULL, NULL);
    if (ret < 0 || obj->src_ctx == NULL) {
        printf("open stream: '%s', error code: %d \n", obj->src_path, ret);
        return -1;
    }


    // to find the streams info
    ret = avformat_find_stream_info(obj->src_ctx, NULL);
    if (ret < 0) {
        printf("could not find stream info.\n");
        return -1;
    }

    // Find audio/video stream.
    obj->video_stream_index = -1;
    obj->audio_stream_index = -1;
    
    for (i = 0; i < obj->src_ctx->nb_streams; i++) {
        if (obj->video_stream_index == -1 && obj->src_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            obj->video_stream_index = i;
            obj->input_video_timebase_num = obj->src_ctx->streams[i]->codec->time_base.num;
            obj->input_video_timebase_den = obj->src_ctx->streams[i]->codec->time_base.den;  // should not be 0

            obj->input_video_stream_timebase_num = obj->src_ctx->streams[i]->time_base.num;
            obj->input_video_stream_timebase_den = obj->src_ctx->streams[i]->time_base.den;  // should not be 0   

            
        }
        if (obj->audio_stream_index == -1 && obj->src_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            obj->audio_stream_index = i;
            obj->input_audio_stream_timebase_num = obj->src_ctx->streams[i]->time_base.num;
            obj->input_audio_stream_timebase_den = obj->src_ctx->streams[i]->time_base.den;  // should not be 0   

        }
    }


    av_dump_format(obj->src_ctx, 0, obj->src_ctx->filename, 0);

    return 0;
}



int main(int argc, char ** argv)
{
    printf("hello lsp\n");

    int ret ;
    struct object  main_obj;
    struct object * obj = &main_obj;

    struct Lsp_args args;


    Lsp_handle handle = Lsp_create();
    printf("get handle\n");

    // init the args
    memset(&args, 0, sizeof(struct Lsp_args));
    memset(obj, 0, sizeof(struct object));


    strcpy(obj->src_path, "/root/video/Meerkats.flv");



    // set the params which should must be set.
    args.have_video_stream = 1;
    args.have_audio_stream = 1;

    //strcpy(&args.stream_out_path, "/root/video/out.flv");
    strcpy(&args.stream_out_path, "rtmp://0.0.0.0/live/test");
    args.i_channels = 2;
    args.i_sample_rate = 44100;
    args.i_sample_fmt = AUDIO_SAMPLE_FORMAT_TYPE_U8;




    av_register_all();
    // open the source file to get video and audio data
    if(open_input_media(obj) < 0){
        printf("open_input_media(%s) error, return\n", obj->src_path);
        return -1;
    }
    printf("open_input_media(%s) over.\n", obj->src_path);

    // set vide/audio extradata
    AVCodecContext * c = NULL;
    c = obj->src_ctx->streams[obj->video_stream_index]->codec;
    if(c->extradata_size > 0 && c->extradata_size < 256){
        args.vextradata_size = c->extradata_size;
        memcpy(args.vextradata, c->extradata, c->extradata_size);
    }
    c = obj->src_ctx->streams[obj->audio_stream_index]->codec;
    if(c->extradata_size > 0 && c->extradata_size < 128){
        args.aextradata_size = c->extradata_size;
        memcpy(args.aextradata, c->extradata, c->extradata_size);
    }


    ret = Lsp_init(handle, &args);
    printf("Lsp_init return:%d\n", ret);
    if (ret < 0){
        return -1;
    }

    AVPacket *pkt = NULL;
    Lsp_data xdata;
    while(1){
        if(pkt == NULL){
            pkt = malloc(sizeof(AVPacket));
            if (pkt == NULL) {
                printf("malloc failed for video pkt");
                exit(-1);
            }
        }

        av_init_packet(pkt);
        pkt->destruct = av_destruct_packet;
        ret = av_read_frame(obj->src_ctx, pkt);
        if(ret < 0){
            printf("read file over.\n");
            break;
        }
        printf("read a frame, index:%d, size:%d.\n", pkt->stream_index, pkt->size);

        // prepare xchannel data
        memset(&xdata, 0, sizeof(xdata));
        
        if(pkt->stream_index == obj->video_stream_index){
            xdata.type = DATA_TYPE_ENC_VIDEO;
            xdata.timebase = 1000;
            xdata.pts = pkt->pts;
            xdata.dts = pkt->dts;
            xdata.size = pkt->size;
            xdata.data = pkt->data;
            xdata.is_key = pkt->flags & AV_PKT_FLAG_KEY;
            
        }
        else if(pkt->stream_index == obj->audio_stream_index){

            // if decode
            
            //continue;
            xdata.type = DATA_TYPE_ENC_AUDIO;
            xdata.timebase = 1000;
            xdata.pts = pkt->pts;
            xdata.dts = pkt->dts;
            xdata.size = pkt->size;
            xdata.data = pkt->data;
            xdata.is_key = pkt->flags & AV_PKT_FLAG_KEY;

        }
        else{
            printf("not support stream idx(%d)\n", pkt->stream_index);
            break;
        }

        ret = Lsp_push(handle, &xdata);
        if(ret < 0){
            printf("Lsp_push err(%d). \n", ret);
            return -1;
        }else{
            printf("send a frame ok, index:%d, size:%d.\n", pkt->stream_index, pkt->size);

        }

        msSleep(10);
    }

    return 0;
}