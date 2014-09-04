
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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


static int decode_video_packet(AVCodecContext * ctx, AVPacket * packet,  AVFrame * picture, int * got_frame)
{
    printf("[debug] decode video pkt.\n");
    avcodec_get_frame_defaults(picture);


    int got_picture;

    *got_frame = 0;

    int ret = avcodec_decode_video2(ctx, picture, &got_picture, packet);
    if (ret < 0) {
        printf("[error] video frame decode error for pkt: %"PRId64" \n", packet->pts);
        return -1;
    }

    int64_t pts = 0;
    //if (picture->opaque && *(int64_t*) picture->opaque != AV_NOPTS_VALUE) {
     //   pts = *(int64_t *) picture->opaque;
    //}

    if(!got_picture){
        printf("[error] no picture gotten.\n");
        return -1;
    }else{
        *got_frame = 1;
        picture->pts = pts;
        picture->pict_type = 0;
        printf("[debug] got video pic, type:%d, pts:%"PRId64" \n", picture->pict_type, picture->pts);

    }

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
    int input_video_timebase_num  = 0;
    int input_video_timebase_den  = 0;
    int input_video_stream_timebase_num  = 0;
    int input_video_stream_timebase_den  = 0;
    int input_video_ticks_per_frame = 1;
    int input_fps_num = 1;
    int input_fps_den = 1;
    int jietime_by_timebase;
    AVFrame picture;
    int got_frame = 0;

    AVFormatContext * outctx = NULL;
    AVOutputFormat  * fmt = NULL;  // output format
    AVStream        * video_st = NULL;    // output stream
    AVCodecContext  * outVideoCodecCtx = NULL;
    AVCodec * outViedeCodec = NULL;
    int got_encode_frame = 0;

    AVPacket pkt; 
    int not_get_decode_frame = 0;

    if(src == NULL || dest_path == NULL){
        printf("[error]: jietu input params NULL.\n");
        return -1;
    }

    if(jietime <= 0){
        printf("[warning] jietime <= 0, modify it to 0.\n");
        jietime = 0;
    }
    printf("[debug] jietime=%d\n", jietime);
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
            input_video_timebase_num = inctx->streams[i]->codec->time_base.num;
            input_video_timebase_den = inctx->streams[i]->codec->time_base.den;  // should not be 0
            input_video_stream_timebase_num = inctx->streams[i]->time_base.num;
            input_video_stream_timebase_den = inctx->streams[i]->time_base.den;  // should not be 0
            input_video_ticks_per_frame = inctx->streams[i]->codec->ticks_per_frame;

            printf("[debug] w:%d, h:%d, timebase.num:%d, timebase.den:%d, stream_timebase.num:%d, stream_timebase.den:%d, ticks_per_frame:%d\n", 
                inWidth, inHeight, input_video_timebase_num, input_video_timebase_den, 
                input_video_stream_timebase_num, input_video_stream_timebase_den, input_video_ticks_per_frame);

            // trans jietime to video stream timebase.
            if(input_video_stream_timebase_num <= 0 || input_video_stream_timebase_den <= 0){
                printf("[error] get input_video_stream_timebase error.\n");
                return -1;
            }
            jietime_by_timebase = (jietime*input_video_stream_timebase_den/input_video_stream_timebase_num)/1000;

            // get fps
            input_fps_den = inctx->streams[i]->r_frame_rate.den;
            input_fps_num = inctx->streams[i]->r_frame_rate.num;

            printf("[debug] input video fps %d/%d\n", input_fps_num, input_fps_den);
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

    if(avformat_write_header(outctx, NULL)){
        printf("[error] outctx av_write_header error!\n");
        return -1;
    }


    // seek the jietu time, AV_TIME_BASE=1000000 
    ret = av_seek_frame(inctx, -1, jietime*1000, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        printf("[error] could not seek to position %d\n", jietime);
        return -1;
    }

    // get the input stream video should interval, interval=timebase/framerate, e.g. 1000/25=40ms
    int video_should_interval = 40;
    if(input_video_timebase_den > 0 && input_video_stream_timebase_den > 0 && input_video_timebase_num > 0 && input_video_stream_timebase_num > 0 && input_video_ticks_per_frame > 0){
        //video_should_interval = (input_video_stream_timebase_den/input_video_stream_timebase_num)/(input_video_timebase_den/(input_video_timebase_num*input_video_ticks_per_frame));
        video_should_interval = (input_video_stream_timebase_den/input_video_stream_timebase_num) / (input_fps_num/input_fps_den);
    }
    printf("[debug] video_should_interval=%d, jietime_by_timebase=%d\n", video_should_interval,jietime_by_timebase);

    not_get_decode_frame = 0;
    // get the frame
    while(1){
        av_init_packet(&pkt);
        pkt.data = NULL;
        pkt.size = 0;
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
            printf("[debug] pkt dts: %lld ,pts: %lld, id_key:%d \n", pkt.dts, pkt.pts, pkt.flags & AV_PKT_FLAG_KEY);
            // decode the video frame
            ret = decode_video_packet(inctx->streams[input_video_stream_index]->codec, &pkt, &picture, &got_frame);
            if(ret < 0 || got_frame==0){
                printf("[debug] decode video , bug not get frame.\n");
                //return -1;
                not_get_decode_frame += 1;
                if(not_get_decode_frame > 10){
                    printf("[error] long times for not_get_decode_frame:%d\n", not_get_decode_frame);
                    return -1;
                }
                continue;
            }


            // if framerate=25, dts should: 1:0, 2:40, 3:80, 4:120
            if(jietime_by_timebase >= (pkt.dts - video_should_interval*not_get_decode_frame)
                && jietime_by_timebase < (pkt.dts + video_should_interval - video_should_interval*not_get_decode_frame)){
                printf("[debug] get the jietu frame, picture pts: %lld\n", (pkt.dts - video_should_interval*not_get_decode_frame));
                // encode the picture
                av_init_packet(&pkt);
                pkt.data = NULL;
                pkt.size = 0;
                ret = avcodec_encode_video2(outVideoCodecCtx, &pkt, &picture, &got_encode_frame);
                if(ret < 0 || got_encode_frame==0){
                    printf("[error] encode picture error.\n");
                    return -1;
                }

                // write the pkt to stream
                pkt.stream_index = input_video_stream_index;
                ret = av_write_frame(outctx, &pkt);
                av_write_trailer(outctx);

                break;
            }
        }
    }

    // close ctx

    return 0;
}



/*
    Function: jieshipin
    Input:  
        src : source file path
        starttime: the start time of the stream which you want to get. unit is ms, e.g. 4000 means 4 s.
        endtime  : the end   time of the stream which you want to get. unit is ms, e.g. 4000 means 4 s.  
        dest_path: the dest file path

    Return:
        0 : means ok
        not 0 : means error
*/
int jieshipin(char * src, int starttime, int endtime, char * dest_path)
{
    int ret;
    int i;

    AVFormatContext * inctx = NULL;
    int input_video_stream_index = -1;
    int input_audio_stream_index = -1;
    AVCodecContext  * inVideoCodecCtx = NULL;
    int inWidth  = 0;
    int inHeight = 0;
    AVCodec * inViedeCodec = NULL;
    int input_video_timebase_num  = 0;
    int input_video_timebase_den  = 0;
    int input_video_stream_timebase_num  = 0;
    int input_video_stream_timebase_den  = 0;
    int input_video_ticks_per_frame = 1;
    int input_fps_num = 1;
    int input_fps_den = 1;
    int jietime_by_timebase;    // end time in video timebase
    AVFrame picture;
    int got_frame = 0;

    AVFormatContext * outctx = NULL;
    AVOutputFormat  * fmt = NULL;  // output format
    AVStream        * video_st = NULL;    // output stream
    AVCodecContext  * outVideoCodecCtx = NULL;
    AVCodec * outViedeCodec = NULL;
    int got_encode_frame = 0;

    AVPacket pkt; 
    int not_get_decode_frame = 0;
    int64_t frist_video_packet_dts = 0;
    int64_t frist_audio_packet_dts = 0;

    if(src == NULL || dest_path == NULL){
        printf("[error]: jietu input params NULL.\n");
        return -1;
    }

    if(starttime <= 0){
        printf("[warning] starttime <= 0, modify it to 0.\n");
        starttime = 0;
    }
    printf("[debug] starttime=%d\n", starttime);



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
            input_video_timebase_num = inctx->streams[i]->codec->time_base.num;
            input_video_timebase_den = inctx->streams[i]->codec->time_base.den;  // should not be 0
            input_video_stream_timebase_num = inctx->streams[i]->time_base.num;
            input_video_stream_timebase_den = inctx->streams[i]->time_base.den;  // should not be 0
            input_video_ticks_per_frame = inctx->streams[i]->codec->ticks_per_frame;

            printf("[debug] w:%d, h:%d, timebase.num:%d, timebase.den:%d, stream_timebase.num:%d, stream_timebase.den:%d, ticks_per_frame:%d\n", 
                inWidth, inHeight, input_video_timebase_num, input_video_timebase_den, 
                input_video_stream_timebase_num, input_video_stream_timebase_den, input_video_ticks_per_frame);

            // trans jietime to video stream timebase.
            if(input_video_stream_timebase_num <= 0 || input_video_stream_timebase_den <= 0){
                printf("[error] get input_video_stream_timebase error.\n");
                return -1;
            }
            jietime_by_timebase = (endtime*input_video_stream_timebase_den/input_video_stream_timebase_num)/1000;

            // get fps
            input_fps_den = inctx->streams[i]->r_frame_rate.den;
            input_fps_num = inctx->streams[i]->r_frame_rate.num;

            printf("[debug] input video fps %d/%d\n", input_fps_num, input_fps_den);
            
        }
        if(input_audio_stream_index == -1 && inctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){

            input_audio_stream_index = i;
            printf("[debug] input_audio_stream_index=%d\n", input_audio_stream_index);
        }
    }

    if(input_video_stream_index == -1){
        printf("[error] not found video stream.\n");
        return -1;
    }

    /*  
    //// fix me: decode is too slow, not use yet
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
    */

    //// open the output file
    outctx = avformat_alloc_context();
    if(strncmp(inctx->iformat->name, "ts", sizeof("ts")) == 0){
        fmt = av_guess_format("mpegts", NULL, NULL);
    } else if(strncmp(inctx->iformat->name, "flv", sizeof("flv")) == 0){
        fmt = av_guess_format("flv", NULL, NULL);
    } else {
        fmt = av_guess_format("mp4", NULL, NULL);
    }
    if (!fmt) {
        printf("[error] Could not find %s format", inctx->iformat->name);
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



    AVStream *video_stream;
    AVStream *audio_stream;

    int video_extra_size;

    //AVMetadataTag *t = NULL;
    //while ((t = av_metadata_get(ctx->metadata, "", t, AV_METADATA_IGNORE_SUFFIX)))
    //    av_metadata_set2(&outctx->metadata, t->key, t->value, AV_METADATA_DONT_OVERWRITE);

    if(video_st != NULL){

        video_st->time_base = inctx->streams[input_video_stream_index]->time_base;
        avcodec_get_context_defaults3(video_st->codec, NULL);
        outVideoCodecCtx = video_st->codec;

        outVideoCodecCtx->codec_id = inctx->streams[input_video_stream_index]->codec->codec_id;
        outVideoCodecCtx->codec_type = inctx->streams[input_video_stream_index]->codec->codec_type;
        /*
        (!outVideoCodecCtx->codec_tag){
                if( !outctx->oformat->codec_tag
                   || av_codec_get_id (outctx->oformat->codec_tag, inctx->streams[input_video_stream_index]->codec->codec_tag) == outVideoCodecCtx->codec_id
                   || av_codec_get_tag(outctx->oformat->codec_tag, inctx->streams[input_video_stream_index]->codec->codec_id) <= 0)
                    outVideoCodecCtx->codec_tag = inctx->streams[i]->codec->codec_tag;
        }
        */
        outVideoCodecCtx->bit_rate = inctx->streams[input_video_stream_index]->codec->bit_rate;
        outVideoCodecCtx->bit_rate_tolerance = inctx->streams[input_video_stream_index]->codec->bit_rate_tolerance;

        outVideoCodecCtx->rc_buffer_size = inctx->streams[input_video_stream_index]->codec->rc_buffer_size;
        outVideoCodecCtx->pix_fmt = inctx->streams[input_video_stream_index]->codec->pix_fmt;
        outVideoCodecCtx->time_base = inctx->streams[input_video_stream_index]->codec->time_base;  

        outVideoCodecCtx->width = inctx->streams[input_video_stream_index]->codec->width;
       // outVideoCodecCtx->width = 640;
        outVideoCodecCtx->height = inctx->streams[input_video_stream_index]->codec->height;
       // outVideoCodecCtx->height = 480;
        outVideoCodecCtx->has_b_frames = inctx->streams[input_video_stream_index]->codec->has_b_frames;
        printf("width:%d, height:%d\n", outVideoCodecCtx->width, outVideoCodecCtx->height);
        
        if(outctx->oformat->flags & AVFMT_GLOBALHEADER)
            outVideoCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;

        video_extra_size = inctx->streams[input_video_stream_index]->codec->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE;
        outVideoCodecCtx->extradata = av_mallocz(video_extra_size);
        memcpy(outVideoCodecCtx->extradata, inctx->streams[input_video_stream_index]->codec->extradata, inctx->streams[input_video_stream_index]->codec->extradata_size);
        outVideoCodecCtx->extradata_size = inctx->streams[input_video_stream_index]->codec->extradata_size;
    }

    av_dump_format(outctx, 0, dest_path, 1);  

    /*
    //// fix me: encode video is too slow, not use yet
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
    */

    if(avformat_write_header(outctx, NULL)){
        printf("[error] outctx av_write_header error!\n");
        return -1;
    }
    

    // seek the jietu time, AV_TIME_BASE=1000000 
    ret = av_seek_frame(inctx, -1, starttime*1000, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        printf("[error] could not seek to position %d\n", starttime);
        return -1;
    }

    // get the input stream video should interval, interval=timebase/framerate, e.g. 1000/25=40ms
    int video_should_interval = 40;
    if(input_video_timebase_den > 0 && input_video_stream_timebase_den > 0 && input_video_timebase_num > 0 && input_video_stream_timebase_num > 0 && input_video_ticks_per_frame > 0){
        //video_should_interval = (input_video_stream_timebase_den/input_video_stream_timebase_num)/(input_video_timebase_den/(input_video_timebase_num*input_video_ticks_per_frame));
        video_should_interval = (input_video_stream_timebase_den/input_video_stream_timebase_num) / (input_fps_num/input_fps_den);
    }
    printf("[debug] video_should_interval=%d, jietime_by_timebase=%d\n", video_should_interval,jietime_by_timebase);

    not_get_decode_frame = 0;
    // get the frame
    while(1){
        av_init_packet(&pkt);
        pkt.data = NULL;
        pkt.size = 0;
        ret = av_read_frame(inctx, &pkt);
        if(ret == AVERROR(EAGAIN)){
            // EAGAIN means try again
            printf("[warning] av_read_frame ret=%d, continue.\n", ret);
            continue;
        }
        if(ret == AVERROR_EOF){
            printf("[warning] av_read_frame ret=AVERROR_EOF, break.\n");
            break;
        }
        if(ret < 0){
            printf("[error] av_read_frame error, ret=%d.\n",ret);
            return -1;
        }

        if(pkt.stream_index == input_video_stream_index){
            printf("[debug] video pkt dts: %lld ,pts: %lld, is_key:%d \n", pkt.dts, pkt.pts, pkt.flags & AV_PKT_FLAG_KEY);
            /*
            // decode the video frame
            ret = decode_video_packet(inctx->streams[input_video_stream_index]->codec, &pkt, &picture, &got_frame);
            if(ret < 0 || got_frame==0){
                printf("[debug] decode video , bug not get frame.\n");
                //return -1;
                not_get_decode_frame += 1;
                if(not_get_decode_frame > 10){
                    printf("[error] long times for not_get_decode_frame:%d\n", not_get_decode_frame);
                    return -1;
                }
                continue;
            }
            */
            if(frist_video_packet_dts == 0){
                frist_video_packet_dts = pkt.dts;
            }
            // break until dts >= endtime
            if(pkt.dts < jietime_by_timebase){
                //printf("[debug] get the jietu frame, picture pts: %lld\n", (pkt.dts - video_should_interval*not_get_decode_frame));
                /*
                // encode the picture
                av_init_packet(&pkt);
                pkt.data = NULL;
                pkt.size = 0;
                ret = avcodec_encode_video2(outVideoCodecCtx, &pkt, &picture, &got_encode_frame);
                if(ret < 0 || got_encode_frame==0){
                    printf("[error] encode picture error.\n");
                    return -1;
                }
                */

                // write the pkt to stream
                pkt.stream_index = input_video_stream_index;
                pkt.dts = pkt.dts - frist_video_packet_dts;
                pkt.pts = pkt.pts - frist_video_packet_dts + 2*video_should_interval;
                printf("[debug] video pkt write. pts=%lld, dts=%lld\n", pkt.pts, pkt.dts);
                ret = av_interleaved_write_frame(outctx, &pkt);
                if (ret < 0) {
                    printf("[error] Could not write frame of stream: %d\n", ret);
                    return -1;
                }
                //av_write_trailer(outctx);
            }else{
                printf("[debug] at the end time, stop.\n");
                break;
            }
        }else if(pkt.stream_index == input_audio_stream_index){
            printf("[debug] audio pkt dts: %lld ,pts: %lld, is_key:%d \n", pkt.dts, pkt.pts, pkt.flags & AV_PKT_FLAG_KEY);
            continue;

            if(frist_audio_packet_dts == 0){
                frist_audio_packet_dts = pkt.dts;
            }
            // if audio pkt , write
            pkt.stream_index = input_audio_stream_index;
            pkt.dts = pkt.dts - frist_audio_packet_dts;
            pkt.pts = pkt.pts - frist_audio_packet_dts + 2*video_should_interval;
            printf("[debug] audio pkt write. pts=%lld, dts=%lld\n", pkt.pts, pkt.dts);
            ret = av_interleaved_write_frame(outctx, &pkt);
            if (ret < 0) {
                printf("[error] Could not write frame of stream: %d\n", ret);
                return -1;
            }
        }
    }

    // end of the stream
    av_write_trailer(outctx);

    // close ctx


    return 0;
}


int main(int argc, char ** argv)
{
    int ret = 0;

    //ret = zongshijian(argv[1]);
    //ret = zongshijian("/home/tvie/lbld_720p_v3min.mp4");
    //printf("get duration:%d.\n",ret);

   // ret = jietu(argv[1], atoi(argv[2]), "/tmp/jietu.jpg");
    //ret = jietu("/home/tvie/lbld_720p_v3min.mp4", 40*34, "/tmp/jietu.jpg");
    //printf("jietu:%d.\n",ret);

    ret = jieshipin("/tmp/shouji1.mp4", 100, 8000, "/tmp/jieshipin.mp4");
    printf("jieshipin: %d\n", ret);

    return 0;
}
