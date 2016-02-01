

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>


#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"



/****  Defines *****/
static int Glb_Log_Level = 4;
#define LOG_DEBUG(format, arg...)    \
    if (Glb_Log_Level >=4) {    \
        time_t Log_Timer = time(NULL); \
        struct tm * Log_tm_ptr = localtime(&Log_Timer);\
        printf("[%d-%d-%d %d:%d:%d][debug][F:%s][L:%u] " format, \
            (1900+Log_tm_ptr->tm_year), (1+Log_tm_ptr->tm_mon), Log_tm_ptr->tm_mday, \
            Log_tm_ptr->tm_hour, Log_tm_ptr->tm_min, Log_tm_ptr->tm_sec, \
            __FUNCTION__ ,__LINE__,  ## arg ); \
    }

#define LOG_INFO(format, arg...)    \
    if (Glb_Log_Level >=3) {    \
        time_t Log_Timer = time(NULL); \
        struct tm * Log_tm_ptr = localtime(&Log_Timer);\
        printf("[%d-%d-%d %d:%d:%d][info][F:%s][L:%u] " format, \
            (1900+Log_tm_ptr->tm_year), (1+Log_tm_ptr->tm_mon), Log_tm_ptr->tm_mday, \
            Log_tm_ptr->tm_hour, Log_tm_ptr->tm_min, Log_tm_ptr->tm_sec, \
            __FUNCTION__ ,__LINE__,  ## arg ); \
    }

#define LOG_WARN(format, arg...)    \
    if (Glb_Log_Level >=2) {    \
        time_t Log_Timer = time(NULL); \
        struct tm * Log_tm_ptr = localtime(&Log_Timer);\
        printf("[%d-%d-%d %d:%d:%d][warn][F:%s][L:%u] " format, \
            (1900+Log_tm_ptr->tm_year), (1+Log_tm_ptr->tm_mon), Log_tm_ptr->tm_mday, \
            Log_tm_ptr->tm_hour, Log_tm_ptr->tm_min, Log_tm_ptr->tm_sec, \
            __FUNCTION__ ,__LINE__,  ## arg ); \
    }

#define LOG_ERROR(format, arg...)    \
    if (Glb_Log_Level >=1) {    \
        time_t Log_Timer = time(NULL); \
        struct tm * Log_tm_ptr = localtime(&Log_Timer);\
        printf("[%d-%d-%d %d:%d:%d][error][F:%s][L:%u] " format, \
            (1900+Log_tm_ptr->tm_year), (1+Log_tm_ptr->tm_mon), Log_tm_ptr->tm_mday, \
            Log_tm_ptr->tm_hour, Log_tm_ptr->tm_min, Log_tm_ptr->tm_sec, \
            __FUNCTION__ ,__LINE__,  ## arg ); \
    }


#define SLICER_MODE_ENCODE_ONLY             0
#define SLICER_MODE_COPY_ONLY               1
#define SLICER_MODE_ENCODE_COPY_ENCODE      2


    
typedef struct {
    /* the input media url */
    char input_url[256];

    /* the output media url */
    char output_url[256];

    /* start time, times units is ms*/
    int start_time;

    /* end time, times units is ms*/
    int end_time;

    /* slice mode*/
    int slice_mode;

    /* log levle */
    int log_level;

    /**/
    char output_format[32];

} Slicer_Params_t;


typedef struct {
    /* slicer params */
    Slicer_Params_t params;

    /* the input media context */
    AVFormatContext * input_ctx;

    /* the ouput media context */
    AVFormatContext * output_ctx;

} Slicer_t;




/******  Functions ******/

static void print_hex(char * phex, int size)
{
    if(phex == NULL)
        return;

    while(size>0){
        printf("%x ", phex[size]);
        size--;
    }

    printf("\n");
}


static void Slicer_params_default(Slicer_Params_t * params)
{
    memset(params, 0, sizeof(Slicer_Params_t));
    params->slice_mode = SLICER_MODE_COPY_ONLY;
    params->log_level = 4;
}


static int Slicer_open_input_media(Slicer_t *obj)
{
    if(obj == NULL){
        LOG_ERROR("Slicer_open_input_media input arg: obj is NULL\n");
        return -1;
    }

    int ret = 0;

    obj->input_ctx = NULL;
    ret = avformat_open_input(&obj->input_ctx, obj->params.input_url, NULL, NULL);
    if (ret < 0 || obj->input_ctx == NULL) {
        LOG_ERROR("open stream: '%s', error code: %d \n", obj->params.input_url, ret);
        return -1;
    }

    ret = avformat_find_stream_info(obj->input_ctx, NULL);
    if (ret < 0) {
        LOG_ERROR("could not find stream info.\n");
        return -1;
    }

    obj->input_ctx->flags |= AVFMT_FLAG_GENPTS;
    av_dump_format(obj->input_ctx, 0, obj->input_ctx->filename, 0);

    // if slice mode need encode, should open the decoder and encoder
    if(obj->params.slice_mode == SLICER_MODE_ENCODE_ONLY || obj->params.slice_mode == SLICER_MODE_ENCODE_COPY_ENCODE){
        LOG_DEBUG("slice mode need encode, to open the decoder.\n");

        // open the decoder


    }else{
        LOG_DEBUG("slice mode no need encode, just copy it.\n");
    }

    return 0;
}


static int Slicer_open_output_media(Slicer_t *obj)
{
    if(obj == NULL){
        LOG_ERROR("Slicer_open_output_media input arg: obj is NULL\n");
        return -1;
    }

    int ret = 0;
    AVOutputFormat  * fmt = NULL;
    obj->output_ctx = NULL;
    

    avformat_alloc_output_context2(&obj->output_ctx, NULL, NULL, obj->params.output_url);
    if (!obj->output_ctx) {
        LOG_ERROR("Could not create output context\n");
        return -1;
    }
    /*
    obj->output_ctx = avformat_alloc_context();
    LOG_DEBUG("input_ctx->iformat->name:%s", obj->input_ctx->iformat->name);
    if(strncmp(obj->input_ctx->iformat->name, "ts", sizeof("ts")) == 0){
        fmt = av_guess_format("mpegts", NULL, NULL);
    } else if(strncmp(obj->input_ctx->iformat->name, "flv", sizeof("flv")) == 0){
        fmt = av_guess_format("flv", NULL, NULL);
    } else {
        fmt = av_guess_format("mp4", NULL, NULL);
    }

    fmt = av_guess_format(obj->input_ctx->iformat->name, NULL, NULL);
    if (!fmt) {
        LOG_DEBUG("[error] Could not find %s format", obj->input_ctx->iformat->name);
        return -1;
    }

    obj->output_ctx->oformat = fmt;  
    */


    int i;
    for (i = 0; i < obj->input_ctx->nb_streams; i++) {
        AVStream *in_stream = obj->input_ctx->streams[i];
        AVStream *out_stream = avformat_new_stream(obj->output_ctx, in_stream->codec->codec);
        if (!out_stream) {
            LOG_ERROR("Failed allocating output stream\n");
            return -1;
        }

        ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
        if (ret < 0) {
            LOG_ERROR("Failed to copy context from input to output stream codec context\n");
            return -1;
        }
        if (obj->output_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }
    av_dump_format(obj->output_ctx, 0, obj->params.output_url, 1);



    if (avio_open(&obj->output_ctx->pb, obj->params.output_url, AVIO_FLAG_READ_WRITE) < 0){
        LOG_ERROR("open output file:%s error.\n", obj->params.output_url);
        return -1;
    }

    // if slice mode need encode, should open the decoder and encoder
    if(obj->params.slice_mode == SLICER_MODE_ENCODE_ONLY || obj->params.slice_mode == SLICER_MODE_ENCODE_COPY_ENCODE){
        LOG_DEBUG("slice mode need encode, to open the encoder.\n");

        // open the encoder


    }else{
        LOG_DEBUG("slice mode no need encode, just copy it.\n");
    }

    return 0;
}

/** init the slicer **/
static int Slicer_init(Slicer_t *obj, Slicer_Params_t * params)
{

    // simple check the input args
    if(obj == NULL){
        LOG_ERROR("Slicer_init input arg: obj is NULL\n");
        return -1;
    }
    if(params == NULL){
        LOG_ERROR("Slicer_init input arg: params is NULL\n");
        return -1;
    }

    /** get default params **/
    Slicer_params_default(&obj->params);


    /** update the params **/
    if(strlen(params->input_url) > 0){
        LOG_INFO("param input_url:%s\n", params->input_url);
        strcpy(obj->params.input_url, params->input_url);
    }else{
        LOG_ERROR("param input_url is None\n");
        return -1;
    }

    if(strlen(params->output_url) > 0){
        LOG_INFO("param output_url:%s\n", params->output_url);
        strcpy(obj->params.output_url, params->output_url);
    }else{
        LOG_ERROR("param output_url is None\n");
        return -1;
    }

    if(params->start_time >= 0){
        LOG_INFO("param start_time:%d\n", params->start_time);
        obj->params.start_time = params->start_time;
    }else{
        LOG_ERROR("param start_time:%d is invalid.\n", params->start_time);
        return -1;
    }

    if(params->end_time > 0){
        LOG_INFO("param end_time:%d\n", params->end_time);
        obj->params.end_time = params->end_time;
    }else{
        LOG_ERROR("param end_time:%d is invalid.\n", params->end_time);
        return -1;
    }

    switch(params->slice_mode){
        case SLICER_MODE_ENCODE_ONLY:
            LOG_INFO("param slice_mode: encode only.\n");
            obj->params.slice_mode = SLICER_MODE_ENCODE_ONLY;
            break;
        case SLICER_MODE_COPY_ONLY:
            LOG_INFO("param slice_mode: copy only.\n");
            obj->params.slice_mode = SLICER_MODE_COPY_ONLY;
            break;
        case SLICER_MODE_ENCODE_COPY_ENCODE:
            LOG_INFO("param slice_mode: encode copy encode.\n");
            obj->params.slice_mode = SLICER_MODE_ENCODE_COPY_ENCODE;
            break;
        default:
            LOG_INFO("param slice_mode use default mode: encode only.\n");
            obj->params.slice_mode = SLICER_MODE_ENCODE_ONLY;
    }

    /** register all ffmpeg lib**/
    av_register_all();

    /** open the input media **/
    LOG_DEBUG("To open the input media.\n");
    if(Slicer_open_input_media(obj) < 0){
        LOG_ERROR("open the input media error.\n");
        return -1;
    }

    LOG_DEBUG("To open the input media ok.\n");

    // open the output media
    if(Slicer_open_output_media(obj) < 0){
        LOG_ERROR("open the output media error.\n");
        return -1;
    }

    return 0;
}


int Slicer(Slicer_Params_t * params)
{
    Slicer_t slicer;

    memset(&slicer, 0, sizeof(Slicer_t));


    Slicer_init(&slicer, params);

    // do slice

    // read frame

    // decode video frame

    // encode video frame

    // write frame


    return 0;
}


static int decode_video_packet(AVCodecContext * ctx, AVPacket * packet,  AVFrame * picture, int * got_frame)
{
    //LOG_DEBUG("[debug] decode video pkt.\n");
    avcodec_get_frame_defaults(picture);


    int got_picture;

    *got_frame = 0;

    int ret = avcodec_decode_video2(ctx, picture, &got_picture, packet);
    if (ret < 0) {
        LOG_DEBUG("[error] video frame decode error for pkt: %"PRId64" \n", packet->pts);
        return -1;
    }

    int64_t pts = 0;
    if (picture->opaque && *(int64_t*) picture->opaque != AV_NOPTS_VALUE) {
        pts = *(int64_t *) picture->opaque;
    }

    if(!got_picture){
        LOG_DEBUG("[warnning] no picture gotten.\n");
        return -1;
    }else{
        *got_frame = 1;
        //picture->pts = pts;
        picture->pts = picture->pkt_pts;
        picture->pict_type = 0;
        LOG_DEBUG("[debug] got video pic, type:%d, pts:%"PRId64", pkt_pts:%"PRId64", pkt_dts:%"PRId64". \n", 
            picture->pict_type, picture->pts, picture->pkt_pts, picture->pkt_dts);

    }

    return 0;
}


static void flush_video_frames(AVFormatContext * outctx, AVCodecContext  * outVideoCodecCtx,
                                int input_video_stream_index, int frist_video_packet_dts, int video_should_interval)
{

    int ret = 0;
    int got_encode_frame = 0;
    AVPacket pkt; 


    // encode the picture
    av_init_packet(&pkt);
    pkt.data = NULL;
    pkt.size = 0;


    while(1){
        ret = avcodec_encode_video2(outVideoCodecCtx, &pkt, NULL, &got_encode_frame);
        //if(ret < 0 || got_encode_frame ==0 ){
        if(ret < 0){
            LOG_DEBUG("[error] encode picture error. return:%d\n", ret);
            break;
        }
        
        if(got_encode_frame){
            // write the pkt to stream
            pkt.stream_index = input_video_stream_index;
            pkt.dts = pkt.dts - frist_video_packet_dts;
            pkt.pts = pkt.pts - frist_video_packet_dts + 2*video_should_interval;
            //LOG_DEBUG("[debug] video pkt write. pts=%lld, dts=%lld\n", pkt.pts, pkt.dts);
            ret = av_interleaved_write_frame(outctx, &pkt);
            if (ret < 0) {
                LOG_DEBUG("[error] Could not write frame of stream: %d\n", ret);
                break;
            }
        }else{

            break;
        }
    }

}


/*
    Function: slicer
    Input:  
        src : source file path
        starttime: the start time of the stream which you want to get. unit is ms, e.g. 4000 means 4 s.
        endtime  : the end   time of the stream which you want to get. unit is ms, e.g. 4000 means 4 s.  
        dest_path: the dest file path
        mode: slicer mode(0:encode, 1:copy, 2:encode and copy)

    Return:
        0 : means ok
        not 0 : means error
*/
int slicer2(char * src, int starttime, int endtime, char * dest_path, int mode)
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
    int starttime_by_timebase;    // end time in video timebase
    int jietime_by_timebase;    // end time in video timebase
    AVFrame picture;
    int got_frame = 0;

    AVFormatContext * outctx = NULL;
    AVOutputFormat  * fmt = NULL;  // output format
    AVStream        * video_st = NULL;    // output stream
    AVStream        * audio_st = NULL;    // output stream
    AVCodecContext  * outVideoCodecCtx = NULL;  // video encode codec ctx
    AVCodecContext  * outAudioCodecCtx = NULL;
    AVCodec * outViedeCodec = NULL;
    int got_encode_frame = 0;

    AVPacket pkt; 
    int not_get_decode_frame = 0;
    int64_t frist_video_packet_dts = 0;
    int64_t frist_audio_packet_dts = 0;
    int have_found_start_frame = 0;
    int have_found_end_frame = 0;
    int stream_gop_cnt = 0;
    int reset_stream_ctx_before_copy = 0;
    int reset_stream_ctx_before_end  = 0;

    if(src == NULL || dest_path == NULL){
        LOG_DEBUG("[error]: jietu input params NULL.\n");
        return -1;
    }

    if(starttime <= 0){
        LOG_DEBUG("[warning] starttime <= 0, modify it to 0.\n");
        starttime = 0;
    }
    LOG_DEBUG("[debug] starttime=%d\n", starttime);



    av_register_all();

    //// open the input file
    ret = avformat_open_input(&inctx, src, NULL, NULL);
    if (ret < 0 || inctx == NULL) {
        LOG_DEBUG("[error] error open stream: '%s', error code: %d \n", src, ret);
        return -1;
    }

    ret = avformat_find_stream_info(inctx, NULL);
    if (ret < 0) {
        LOG_DEBUG("[error] could not find stream info.\n");
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

            LOG_DEBUG("[debug] w:%d, h:%d, timebase.num:%d, timebase.den:%d, stream_timebase.num:%d, stream_timebase.den:%d, ticks_per_frame:%d\n", 
                inWidth, inHeight, input_video_timebase_num, input_video_timebase_den, 
                input_video_stream_timebase_num, input_video_stream_timebase_den, input_video_ticks_per_frame);

            // trans jietime to video stream timebase.
            if(input_video_stream_timebase_num <= 0 || input_video_stream_timebase_den <= 0){
                LOG_DEBUG("[error] get input_video_stream_timebase error.\n");
                return -1;
            }
            jietime_by_timebase = (endtime*input_video_stream_timebase_den/input_video_stream_timebase_num)/1000;
            starttime_by_timebase = (starttime*input_video_stream_timebase_den/input_video_stream_timebase_num)/1000;

            // get fps
            input_fps_den = inctx->streams[i]->r_frame_rate.den;
            input_fps_num = inctx->streams[i]->r_frame_rate.num;

            LOG_DEBUG("[debug] input video fps %d/%d\n", input_fps_num, input_fps_den);
            
        }
        if(input_audio_stream_index == -1 && inctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){

            input_audio_stream_index = i;
            LOG_DEBUG("[debug] input_audio_stream_index=%d\n", input_audio_stream_index);
        }
    }

    if(input_video_stream_index == -1){
        LOG_DEBUG("[error] not found video stream.\n");
        return -1;
    }

      
    //// fix me: decode is too slow, not use yet
    // open input video decodec
    inViedeCodec = avcodec_find_decoder(inctx->streams[input_video_stream_index]->codec->codec_id);
    if(inViedeCodec == NULL){
        LOG_DEBUG("[error] inViedeCodec %d not found !\n", inctx->streams[input_video_stream_index]->codec->codec_id);
        return -1;
    }else{
        // open it 
        LOG_DEBUG("[debug] open video decodec codec, id:%d, name:%s\n",inctx->streams[input_video_stream_index]->codec->codec_id, 
            inViedeCodec->name);
        
        ret = avcodec_open2(inctx->streams[input_video_stream_index]->codec, inViedeCodec, NULL);
        if (ret < 0) {
            LOG_DEBUG("[error] could not open video decode codec\n");
            return -1;
        }
    }
    

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
        LOG_DEBUG("Could not find %s format", inctx->iformat->name);
        return -1;
    }

    outctx->oformat = fmt;  

    if (avio_open(&outctx->pb, dest_path, AVIO_FLAG_READ_WRITE) < 0){
        LOG_DEBUG("[error] open output file.\n");
        return -1;
    }



    int video_extra_size;
    char * video_extradata = NULL;
    int audio_extra_size;

    // add video stream
    if(input_video_stream_index != -1){
        video_st = av_new_stream(outctx, outctx->nb_streams);
        if (video_st==NULL){
            LOG_DEBUG("[error] av_new_stream error.\n");
            return -1;
        }
    }
    //AVMetadataTag *t = NULL;
    //while ((t = av_metadata_get(ctx->metadata, "", t, AV_METADATA_IGNORE_SUFFIX)))
    //    av_metadata_set2(&outctx->metadata, t->key, t->value, AV_METADATA_DONT_OVERWRITE);
    AVDictionaryEntry *t = NULL;
    while ((t = av_dict_get(inctx->metadata, "", t, AV_DICT_IGNORE_SUFFIX))){
        //LOG_DEBUG("metadata: %s=%s.\n", t->key, t->value);
        av_dict_set(&outctx->metadata, t->key, t->value, AV_DICT_DONT_OVERWRITE);
    }

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

        if(inctx->streams[input_video_stream_index]->codec->ticks_per_frame > 0)
            outVideoCodecCtx->time_base.den = inctx->streams[input_video_stream_index]->codec->time_base.den / inctx->streams[input_video_stream_index]->codec->ticks_per_frame;  
        else
            outVideoCodecCtx->time_base.den = inctx->streams[input_video_stream_index]->codec->time_base.den;

        outVideoCodecCtx->time_base.num = inctx->streams[input_video_stream_index]->codec->time_base.num;  


        outVideoCodecCtx->width = inctx->streams[input_video_stream_index]->codec->width;
        outVideoCodecCtx->height = inctx->streams[input_video_stream_index]->codec->height;
        LOG_DEBUG("width:%d, height:%d\n", outVideoCodecCtx->width, outVideoCodecCtx->height);

        outVideoCodecCtx->has_b_frames = inctx->streams[input_video_stream_index]->codec->has_b_frames;
        
        outVideoCodecCtx->me_range  = inctx->streams[input_video_stream_index]->codec->me_range;
        if(outVideoCodecCtx->me_range == 0){
            outVideoCodecCtx->me_range = 16;
        }
        LOG_DEBUG("me_range:%d\n", outVideoCodecCtx->me_range);
        
        
        outVideoCodecCtx->max_qdiff = inctx->streams[input_video_stream_index]->codec->max_qdiff;
        LOG_DEBUG("max_qdiff:%d\n", outVideoCodecCtx->max_qdiff);
        
        outVideoCodecCtx->qmin      = inctx->streams[input_video_stream_index]->codec->qmin;
        LOG_DEBUG("qmin:%d\n", outVideoCodecCtx->qmin);
        outVideoCodecCtx->qmax      = inctx->streams[input_video_stream_index]->codec->qmax;
        LOG_DEBUG("qmax:%d\n", outVideoCodecCtx->qmax);
        
        //outVideoCodecCtx->qcompress = inctx->streams[input_video_stream_index]->codec->qcompress;
        outVideoCodecCtx->qcompress = 0.6;
        LOG_DEBUG("qcompress:%f\n", outVideoCodecCtx->qcompress);
        
        
        //if(outctx->oformat->flags & AVFMT_GLOBALHEADER)
        //    outVideoCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;

        //LOG_DEBUG("[debug] enc extra_size=%d\n", outVideoCodecCtx->extradata_size);
        //print_hex(outVideoCodecCtx->extradata,outVideoCodecCtx->extradata_size);

        //video_extra_size = inctx->streams[input_video_stream_index]->codec->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE;
        //outVideoCodecCtx->extradata = av_mallocz(video_extra_size);
        //memcpy(outVideoCodecCtx->extradata, inctx->streams[input_video_stream_index]->codec->extradata, inctx->streams[input_video_stream_index]->codec->extradata_size);
        //outVideoCodecCtx->extradata_size = inctx->streams[input_video_stream_index]->codec->extradata_size;

        //LOG_DEBUG("[debug] enc extra_size=%d\n", outVideoCodecCtx->extradata_size);
        //print_hex(outVideoCodecCtx->extradata,outVideoCodecCtx->extradata_size);
        

        while ((t = av_dict_get(inctx->streams[input_video_stream_index]->metadata, "", t, AV_DICT_IGNORE_SUFFIX))){
            LOG_DEBUG("stream metadata: %s=%s.\n", t->key, t->value);
            av_dict_set(&video_st->metadata, t->key, t->value, AV_DICT_DONT_OVERWRITE);
        }

         
    }

    // add audio stream
    if(input_audio_stream_index != -1){
        audio_st = av_new_stream(outctx, outctx->nb_streams);
        if (audio_st==NULL){
            LOG_DEBUG("[error] av_new_stream error.\n");
            return -1;
        }
    }
    //AVMetadataTag *t = NULL;
    //while ((t = av_metadata_get(ctx->metadata, "", t, AV_METADATA_IGNORE_SUFFIX)))
    //    av_metadata_set2(&outctx->metadata, t->key, t->value, AV_METADATA_DONT_OVERWRITE);

    if(audio_st != NULL){

        audio_st->time_base = inctx->streams[input_audio_stream_index]->time_base;
        avcodec_get_context_defaults3(audio_st->codec, NULL);
        outAudioCodecCtx = audio_st->codec;

        outAudioCodecCtx->codec_id = inctx->streams[input_audio_stream_index]->codec->codec_id;
        outAudioCodecCtx->codec_type = inctx->streams[input_audio_stream_index]->codec->codec_type;

        outAudioCodecCtx->bit_rate = inctx->streams[input_audio_stream_index]->codec->bit_rate;
        outAudioCodecCtx->bit_rate_tolerance = inctx->streams[input_audio_stream_index]->codec->bit_rate_tolerance;

        outAudioCodecCtx->rc_buffer_size = inctx->streams[input_audio_stream_index]->codec->rc_buffer_size;
        //outAudioCodecCtx->pix_fmt = inctx->streams[input_audio_stream_index]->codec->pix_fmt;
        outAudioCodecCtx->time_base = inctx->streams[input_audio_stream_index]->codec->time_base;  

        outAudioCodecCtx->sample_fmt = inctx->streams[input_audio_stream_index]->codec->sample_fmt; 
        outAudioCodecCtx->sample_rate = inctx->streams[input_audio_stream_index]->codec->sample_rate; 
        outAudioCodecCtx->channels = inctx->streams[input_audio_stream_index]->codec->channels; 


        if(outctx->oformat->flags & AVFMT_GLOBALHEADER)
            outAudioCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;

        audio_extra_size = inctx->streams[input_audio_stream_index]->codec->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE;
        outAudioCodecCtx->extradata = av_mallocz(audio_extra_size);
        memcpy(outAudioCodecCtx->extradata, inctx->streams[input_audio_stream_index]->codec->extradata, inctx->streams[input_audio_stream_index]->codec->extradata_size);
        outAudioCodecCtx->extradata_size = inctx->streams[input_audio_stream_index]->codec->extradata_size;
    }
     

    
    //// fix me: encode video is too slow, not use yet
    // open the output encoder
    outViedeCodec = avcodec_find_encoder(outVideoCodecCtx->codec_id);
    if(outViedeCodec == NULL){
        LOG_DEBUG("[error] outViedeCodec %d not found !\n", outVideoCodecCtx->codec_id);
        return -1;
    }else{
        // open it 
        LOG_DEBUG("[debug] open video encodec codec, id:%d, name:%s\n",outVideoCodecCtx->codec_id, outViedeCodec->name);
        AVDictionary *video_encoder_options = NULL;
        if(outVideoCodecCtx->codec_id == AV_CODEC_ID_H264){
             av_dict_set(&video_encoder_options, "x264-params", "sps-id=2", 0);   
        }
    
        ret = avcodec_open2(outVideoCodecCtx, outViedeCodec, video_encoder_options);
        if (ret < 0) {
            LOG_DEBUG("[error] could not open video encode codec\n");
            return -1;
        }

        LOG_DEBUG("[debug] after open, enc extra_size=%d\n", outVideoCodecCtx->extradata_size);
        print_hex(outVideoCodecCtx->extradata,outVideoCodecCtx->extradata_size);


        AVIOContext *pb = NULL;
        char *buf = NULL;
        int size = 0;
        
        ret = avio_open_dyn_buf(&pb);
        if (ret < 0)
            return ret;
        
        ff_isom_write_avcc(pb, outVideoCodecCtx->extradata, outVideoCodecCtx->extradata_size);
        size = avio_close_dyn_buf(pb, &buf);
        
        //video_extra_size = outVideoCodecCtx->extradata_size + inctx->streams[input_video_stream_index]->codec->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE;
        //video_extradata  = av_mallocz(video_extra_size);
        //memcpy(video_extradata, outVideoCodecCtx->extradata, outVideoCodecCtx->extradata_size);
        //memcpy(video_extradata+outVideoCodecCtx->extradata_size, inctx->streams[input_video_stream_index]->codec->extradata, inctx->streams[input_video_stream_index]->codec->extradata_size);

        video_extra_size = size + inctx->streams[input_video_stream_index]->codec->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE;
        video_extradata  = av_mallocz(video_extra_size);

        memcpy(video_extradata, buf, size);
        memcpy(video_extradata+size, outVideoCodecCtx->extradata, outVideoCodecCtx->extradata_size);


        outVideoCodecCtx->extradata_size = size + inctx->streams[input_video_stream_index]->codec->extradata_size;
        outVideoCodecCtx->extradata = video_extradata;
        LOG_DEBUG("[debug] after open rebuild, enc extra_size=%d\n", outVideoCodecCtx->extradata_size);
        print_hex(outVideoCodecCtx->extradata,outVideoCodecCtx->extradata_size);

        
    }
    
    av_dump_format(outctx, 0, dest_path, 1); 

    if(avformat_write_header(outctx, NULL)){
        LOG_DEBUG("[error] outctx av_write_header error!\n");
        return -1;
    }
    

    // seek the jietu time, AV_TIME_BASE=1000000 
    ret = av_seek_frame(inctx, -1, starttime*1000, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        LOG_DEBUG("[error] could not seek to position %d\n", starttime);
        return -1;
    }

    // get the input stream video should interval, interval=timebase/framerate, e.g. 1000/25=40ms
    int video_should_interval = 40;
    if(input_video_timebase_den > 0 && input_video_stream_timebase_den > 0 && input_video_timebase_num > 0 && input_video_stream_timebase_num > 0 && input_video_ticks_per_frame > 0){
        //video_should_interval = (input_video_stream_timebase_den/input_video_stream_timebase_num)/(input_video_timebase_den/(input_video_timebase_num*input_video_ticks_per_frame));
        video_should_interval = (input_video_stream_timebase_den/input_video_stream_timebase_num) / (input_fps_num/input_fps_den);
    }
    LOG_DEBUG("[debug] video_should_interval=%d, jietime_by_timebase=%d\n", video_should_interval,jietime_by_timebase);

    not_get_decode_frame = 0;
    have_found_start_frame = 0;
    have_found_end_frame = 0;
    stream_gop_cnt = 0;
    // get the frame
    while(1){
        av_init_packet(&pkt);
        pkt.data = NULL;
        pkt.size = 0;
        ret = av_read_frame(inctx, &pkt);  // the file has been seek to the nearest before I key frame.
        if(ret == AVERROR(EAGAIN)){
            // EAGAIN means try again
            LOG_DEBUG("[warning] av_read_frame ret=%d, continue.\n", ret);
            continue;
        }
        if(ret == AVERROR_EOF){
            LOG_DEBUG("[warning] av_read_frame ret=AVERROR_EOF, break.\n");
            break;
        }
        if(ret < 0){
            LOG_DEBUG("[error] av_read_frame error, ret=%d.\n",ret);
            return -1;
        }


        if (mode == SLICER_MODE_ENCODE_ONLY){
            
            if(pkt.stream_index == input_video_stream_index){
                // video
                LOG_DEBUG("[debug] video pkt dts: %lld ,pts: %lld, is_key:%d \n", pkt.dts, pkt.pts, pkt.flags & AV_PKT_FLAG_KEY);

                // if the frame is key frame and is the start frame, then start copy, else to decode the frames until get the start frame.
                
                if(!have_found_start_frame){
                    if((pkt.flags & AV_PKT_FLAG_KEY) && (pkt.pts >= starttime_by_timebase)){
                        have_found_start_frame = 1;
                        //frist_video_packet_dts = pkt.dts;
                    }

                    if(pkt.flags & AV_PKT_FLAG_KEY){
                        stream_gop_cnt += 1;
                    }
                }

                    
                // decode the video frame
                ret = decode_video_packet(inctx->streams[input_video_stream_index]->codec, &pkt, &picture, &got_frame);
                if(ret < 0 || got_frame==0){
                    LOG_DEBUG("[debug] decode video , but not get frame.\n");
                    
                    not_get_decode_frame += 1;
                    if(not_get_decode_frame > 10){
                        LOG_DEBUG("[error] long times for not_get_decode_frame:%d\n", not_get_decode_frame);
                        return -1;
                    }
                    continue;
                }

                LOG_DEBUG("[debug] decode video , picture pts=%d.\n", picture.pts);

                // start when time reach
                if(picture.pts < starttime_by_timebase){
                    //LOG_DEBUG("[debug] time not ok , now picture pts=%d, start time:%d.\n", picture.pts, starttime_by_timebase);
                    continue;
                }

                if(picture.pts > jietime_by_timebase){
                    LOG_DEBUG("[debug] reach the end time,  endtime=%d\n", jietime_by_timebase);
                    flush_video_frames(outctx, outVideoCodecCtx, input_video_stream_index, frist_video_packet_dts, video_should_interval);
                    break;
                }


                    

                        
                        
                // encode the picture
                av_init_packet(&pkt);
                pkt.data = NULL;
                pkt.size = 0;
                ret = avcodec_encode_video2(outVideoCodecCtx, &pkt, &picture, &got_encode_frame);
                //if(ret < 0 || got_encode_frame ==0 ){
                if(ret < 0){
                    LOG_DEBUG("[error] encode picture error. return:%d\n", ret);
                    return -1;
                }
                


                if(got_encode_frame){
                    // save the first frame pts
                    if(frist_video_packet_dts == 0){
                        frist_video_packet_dts = picture.pts;
                    }
                    // write the pkt to stream
                    pkt.stream_index = input_video_stream_index;
                    pkt.dts = pkt.dts - frist_video_packet_dts;
                    pkt.pts = pkt.pts - frist_video_packet_dts + 2*video_should_interval;
                    //LOG_DEBUG("[debug] video pkt write. pts=%lld, dts=%lld\n", pkt.pts, pkt.dts);
                    ret = av_interleaved_write_frame(outctx, &pkt);
                    if (ret < 0) {
                        LOG_DEBUG("[error] Could not write frame of stream: %d\n", ret);
                        return -1;
                    }
                }

            }else if((pkt.stream_index == input_audio_stream_index) && have_found_start_frame){
                // audio
                LOG_DEBUG("[debug] audio pkt dts: %lld ,pts: %lld, is_key:%d \n", pkt.dts, pkt.pts, pkt.flags & AV_PKT_FLAG_KEY);

                if(frist_audio_packet_dts == 0){
                    frist_audio_packet_dts = pkt.dts;
                }
                // if audio pkt , write
                pkt.stream_index = input_audio_stream_index;
                pkt.dts = pkt.dts - frist_audio_packet_dts;
                pkt.pts = pkt.pts - frist_audio_packet_dts + 2*video_should_interval;
                LOG_DEBUG("[debug] audio pkt write. pts=%lld, dts=%lld\n", pkt.pts, pkt.dts);
                ret = av_interleaved_write_frame(outctx, &pkt);
                if (ret < 0) {
                    LOG_DEBUG("[error] Could not write frame of stream: %d\n", ret);
                    return -1;
                }
            }


        }else if (mode == SLICER_MODE_ENCODE_COPY_ENCODE){

            if(pkt.stream_index == input_video_stream_index){
                LOG_DEBUG("[debug] video pkt dts: %lld ,pts: %lld, is_key:%d \n", pkt.dts, pkt.pts, pkt.flags & AV_PKT_FLAG_KEY);

                // if the frame is key frame and is the start frame, then start copy, else to decode the frames until get the start frame.
                
                if(!have_found_start_frame){
                    if((pkt.flags & AV_PKT_FLAG_KEY) && (pkt.pts >= starttime_by_timebase)){
                        have_found_start_frame = 1;
                        frist_video_packet_dts = pkt.dts;
                    }

                    if(pkt.flags & AV_PKT_FLAG_KEY){
                        stream_gop_cnt += 1;
                    }
                }

                // if the start frame not the Key frame, transcode the stream from start frame to the next GOP. 
                if(!have_found_start_frame && stream_gop_cnt <= 1){
                    //frist_video_packet_dts = pkt.dts;
                    //continue;

                    // decode the stream until find the start frame
                    
                    // decode the video frame
                    ret = decode_video_packet(inctx->streams[input_video_stream_index]->codec, &pkt, &picture, &got_frame);
                    if(ret < 0 || got_frame==0){
                        LOG_DEBUG("[debug] decode video , but not get frame.\n");
                        
                        not_get_decode_frame += 1;
                        if(not_get_decode_frame > 10){
                            LOG_DEBUG("[error] long times for not_get_decode_frame:%d\n", not_get_decode_frame);
                            return -1;
                        }
                        continue;
                    }

                    LOG_DEBUG("[debug] decode video , picture pts=%d.\n", picture.pts);

                    // start when time reach
                    if(picture.pts < starttime_by_timebase){
                        LOG_DEBUG("[debug] time not ok , now picture pts=%d, start time:%d.\n", picture.pts, starttime_by_timebase);
                        continue;
                    }

                    // save the first frame pts
                    if(frist_video_packet_dts == 0){
                        frist_video_packet_dts = picture.pts;
                    }
                    
                    // break until next GOP
                    if(picture.pts < jietime_by_timebase){
                        //LOG_DEBUG("[debug] get the jietu frame, picture pts: %lld\n", (pkt.dts - video_should_interval*not_get_decode_frame));
                        
                        // encode the picture
                        av_init_packet(&pkt);
                        pkt.data = NULL;
                        pkt.size = 0;
                        ret = avcodec_encode_video2(outVideoCodecCtx, &pkt, &picture, &got_encode_frame);
                        //if(ret < 0 || got_encode_frame ==0 ){
                        if(ret < 0){
                            LOG_DEBUG("[error] encode picture error. return:%d\n", ret);
                            return -1;
                        }
                        
                        if(got_encode_frame){
                            // write the pkt to stream
                            pkt.stream_index = input_video_stream_index;
                            pkt.dts = pkt.dts - frist_video_packet_dts;
                            pkt.pts = pkt.pts - frist_video_packet_dts + 2*video_should_interval;
                            LOG_DEBUG("[debug] video pkt write. pts=%lld, dts=%lld\n", pkt.pts, pkt.dts);
                            ret = av_interleaved_write_frame(outctx, &pkt);
                            if (ret < 0) {
                                LOG_DEBUG("[error] Could not write frame of stream: %d\n", ret);
                                return -1;
                            }
                        }

                    }
                }
                else{

                    // copy the frames until end time
                    if(pkt.dts > jietime_by_timebase){
                        LOG_DEBUG("[debug] reach the end time, pkt.pts=%d, dts=%d, endtime=%d\n", pkt.pts, pkt.dts, jietime_by_timebase);
                        break;
                    }
                    
                    // modify the pkt ts
                    pkt.stream_index = input_video_stream_index;
                    pkt.dts = pkt.dts - frist_video_packet_dts;
                    pkt.pts = pkt.pts - frist_video_packet_dts + 2*video_should_interval;
                    LOG_DEBUG("[debug] video pkt write. pts=%lld, dts=%lld\n", pkt.pts, pkt.dts);

                    
                    // write the frame 
                    ret = av_interleaved_write_frame(outctx, &pkt);
                    if (ret < 0) {
                        LOG_DEBUG("[error] Could not write frame of stream: %d\n", ret);
                        return -1;
                    }  

                

                }
            
            }
            else if((pkt.stream_index == input_audio_stream_index) && have_found_start_frame){
                
                LOG_DEBUG("[debug] audio pkt dts: %lld ,pts: %lld, is_key:%d \n", pkt.dts, pkt.pts, pkt.flags & AV_PKT_FLAG_KEY);

                if(frist_audio_packet_dts == 0){
                    frist_audio_packet_dts = pkt.dts;
                }
                // if audio pkt , write
                pkt.stream_index = input_audio_stream_index;
                pkt.dts = pkt.dts - frist_audio_packet_dts;
                pkt.pts = pkt.pts - frist_audio_packet_dts + 2*video_should_interval;
                LOG_DEBUG("[debug] audio pkt write. pts=%lld, dts=%lld\n", pkt.pts, pkt.dts);
                ret = av_interleaved_write_frame(outctx, &pkt);
                if (ret < 0) {
                    LOG_DEBUG("[error] Could not write frame of stream: %d\n", ret);
                    return -1;
                }
            }

        }else{
            // copy only

        }
        


    }


    // fix me: need flush the encoder 

    // end of the stream
    av_write_trailer(outctx);

    // close ctx


    return 0;
}




int main(int argc, char ** argv)
{
    int ret = 0;

    LOG_DEBUG("Usage: ./slicer <input filename> <output filename> <start time> <end time> \n");
    LOG_DEBUG("Need to pay attention to: times units is ms \n");

    Slicer_Params_t params;

    memset(&params, 0, sizeof(Slicer_Params_t));

	//char *input_filename = argv[1];
	//char *output_filename = argv[2];
	//int start_time = atoi(argv[3]);
	//int end_time = atoi(argv[4]);

    strcpy(params.input_url, "/root/Meerkats.mp4");
    strcpy(params.output_url, "out.flv");
    params.start_time = 10;
    params.end_time = 30;
    params.slice_mode = SLICER_MODE_COPY_ONLY;
    
    ret = Slicer(&params);
    LOG_DEBUG("slicer return: %d\n", ret);

    return 0;
}


