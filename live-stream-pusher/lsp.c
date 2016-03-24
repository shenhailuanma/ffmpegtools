#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>


#include "lsp.h"


#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/common.h>




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


#define VAL_IN_RANGE(val, min, max) ((val)<(min) ? (min):((val)>(max) ? (max):(val))) 
// if  in range val is true
#define VAL_IF_IN_RANGE(val, min, max) ((val)<(min) ? 0:((val)>(max) ? 0:1)) 


struct Lsp_object{
    
    int video_frame_cnt;
    int audio_sample_cnt;
    
    // new 
    AVCodecContext *actx; // audio ctx for encode
    AVCodecContext *vctx; // video ctx for encode
    AVFormatContext *outctx;  // output ctx

    int out_video_stream_index;
    int out_audio_stream_index;

    Lsp_args args;
    
    // for rebuild timestamp
    int64_t in_first_vdts;  // input first video dts
    int64_t vdts;           // rebuilt video dts
    int64_t in_first_adts;  // input first audio dts
    int64_t adts;

    int64_t start_time;
    
};






static int _set_default_args(Lsp_args *args)
{
    if(args == NULL){
        LOG_ERROR(" input params error\n");
        return ERROR_CODE_NULL_POINTER;
    }

    memset(args, 0, sizeof(Lsp_args));


    args->vbit_rate = 100000; // 100k
    args->gop       = 50;
    args->o_width = 640;
    args->o_height = 360;

    args->abit_rate = 48000;
    args->o_channels= 2;
    args->o_sample_rate = 44100;

    //strcpy(args->stream_out_path, "rtmp://10.33.2.181/live/xctestzx01");
    //strcpy(args->stream_save_path, "/tmp/xcsave.ts");

    
    return 0;
}


static int _check_iargs_and_update_args(struct Lsp_args * xargs, struct Lsp_args * iargs)
{
    // don't check args pointer, trust it

    if(strlen(iargs->stream_out_path) <= 0){
        return ERROR_CODE_NO_INPUT;
    }
    strcpy(xargs->stream_out_path, iargs->stream_out_path);

    xargs->have_audio_stream = iargs->have_audio_stream;
    xargs->have_video_stream = iargs->have_video_stream;


    if(xargs->have_audio_stream <= 0 && xargs->have_video_stream <= 0){
        return ERROR_CODE_NO_INPUT;
    }

    // check raw input params
    if(xargs->have_audio_stream){
        if(VAL_IF_IN_RANGE(iargs->i_channels, 1, 2)){
            xargs->i_channels = iargs->i_channels; 
        }else{
            return ERROR_CODE_AUDIO_CHANNEL_ERROR;
        }
        
        if(iargs->i_sample_rate == 44100 || iargs->i_sample_rate == 22050){
            xargs->i_sample_rate = iargs->i_sample_rate; 
        }else{
            return ERROR_CODE_AUDIO_SAMPLE_RATE_ERROR;
        }

        if(VAL_IF_IN_RANGE(iargs->i_sample_fmt, AUDIO_SAMPLE_FORMAT_TYPE_U8, AUDIO_SAMPLE_FORMAT_TYPE_S16)){
            xargs->i_sample_fmt = iargs->i_sample_fmt; 
        }else{
            return ERROR_CODE_AUDIO_SAMPLE_FMT_ERROR;
        }

    }





    // check encode params
    if(VAL_IF_IN_RANGE(iargs->vbit_rate, 100000, 1000000)){
        xargs->vbit_rate = iargs->vbit_rate; // 100k
    }

    if(VAL_IF_IN_RANGE(iargs->o_width, 176, 1920)){
        xargs->o_width = iargs->o_width; 
    }else{
        xargs->o_width = 640;
    }

    if(VAL_IF_IN_RANGE(iargs->o_height, 144, 1080)){
        xargs->o_height = iargs->o_height; 
    }else{
        xargs->o_height = 360;
    }

    if(VAL_IF_IN_RANGE(iargs->o_frame_rate, 1, 30)){
        xargs->o_frame_rate = iargs->o_frame_rate; 
    }else{
        xargs->o_frame_rate = 25;
    }

    if(VAL_IF_IN_RANGE(iargs->gop, 25, 250)){
        xargs->gop = iargs->gop; 
    }

    if(VAL_IF_IN_RANGE(iargs->abit_rate, 32, 196)){
        xargs->abit_rate = iargs->abit_rate; 
    }

    if(VAL_IF_IN_RANGE(iargs->o_channels, 2, 2)){ // aac lib only support 2
        xargs->o_channels = iargs->o_channels; 
    }else{
        //xargs->o_channels = iargs->i_channels;
        xargs->o_channels = 2;
    }

    if(iargs->o_sample_rate == 44100 || iargs->o_sample_rate == 22050){
        //xargs->o_sample_rate = iargs->o_sample_rate; 
        xargs->o_sample_rate = 22050;
    }else{
        //xargs->o_sample_rate = iargs->i_sample_rate;
        xargs->o_sample_rate = 22050;
    }

    if(iargs->aextradata_size > 0){
        xargs->aextradata_size = iargs->aextradata_size;
        memcpy(xargs->aextradata, iargs->aextradata, iargs->aextradata_size);
    }else{
        xargs->aextradata[0] = 0x13;
        xargs->aextradata[1] = 0x90;
        xargs->aextradata_size = 2;
    }

    if(iargs->vextradata_size > 0){
        xargs->vextradata_size = iargs->vextradata_size;
        memcpy(xargs->vextradata, iargs->vextradata, iargs->vextradata_size);
    }

    return ERROR_CODE_OK;
}


static int _lsp_interrupt_cb(void *h)
{
    struct Lsp_object * handle = (struct Lsp_object *)h;
    int64_t now = 0;

    now = av_gettime();

    LOG_DEBUG("now(%lld) - start(%lld) = %lld\n", now, handle->start_time, now - handle->start_time );
   
    if(now - handle->start_time > 5000000){
        LOG_ERROR("_lsp_interrupt_cb return.\n");
        return 1;
    }

    return 0;
}

static AVStream *_av_new_stream_my(AVFormatContext *s, int id)
{
    AVStream *st = avformat_new_stream(s, NULL);
    if (st)
        st->id = id;
    return st;
}

static int _set_stream_params(AVFormatContext *oc, int idx, struct Lsp_args * args)
{
    
    AVStream* outStream      = oc->streams[idx];
    AVCodecContext* outCtx   = outStream->codec;
    int extraSize;
    //int offset;
    
    if(outCtx->codec_type == AVMEDIA_TYPE_VIDEO){
        outCtx->codec_id = AV_CODEC_ID_H264;
        LOG_DEBUG("vcodec_id : %d\n", outCtx->codec_id);
    }else if(outCtx->codec_type == AVMEDIA_TYPE_AUDIO){
        outCtx->codec_id = AV_CODEC_ID_AAC;
        LOG_DEBUG("acodec_id : %d\n", outCtx->codec_id);
    }


    
    if(outCtx->codec_type == AVMEDIA_TYPE_AUDIO){
        
        outCtx->bit_rate = args->abit_rate;
        outCtx->sample_rate = args->o_sample_rate;
        outCtx->channels = args->o_channels;
        outCtx->sample_fmt = args->i_sample_fmt;
        
        outCtx->time_base.den = args->o_sample_rate;
        outCtx->time_base.num = 1;
        //c->bit_rate_tolerance = 2 * 1000 * 1000;   //2Mbps
        outCtx->bit_rate_tolerance = 2 * args->abit_rate;
        outCtx->channel_layout = AV_CH_LAYOUT_STEREO;
        
        if (outCtx->codec_id == AV_CODEC_ID_AAC){
            outCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;    //dump sequence header in extradata
        }

        if(outCtx->block_align == 1 && outCtx->codec_id == AV_CODEC_ID_MP3){
            outCtx->block_align = 0;
        }
        if(outCtx->codec_id == AV_CODEC_ID_AC3){
            outCtx->block_align = 0;
        }

        if(args->aextradata_size > 0){
             extraSize = args->aextradata_size + FF_INPUT_BUFFER_PADDING_SIZE;
             outCtx->extradata  = (uint8_t*)av_mallocz(extraSize); // this can be freed by avformat_free_context
             memcpy(outCtx->extradata, args->aextradata, args->aextradata_size);
             outCtx->extradata_size = args->aextradata_size;
        }
    }

    if(outCtx->codec_type == AVMEDIA_TYPE_VIDEO){
        
        outCtx->pix_fmt = args->o_pix_fmt;
        outCtx->width   = args->o_width;
        outCtx->height  = args->o_height;
        outCtx->flags   = args->o_height;

        outCtx->time_base.den    = args->o_frame_rate;
        outCtx->time_base.num    = 1;

        outCtx->bit_rate = args->vbit_rate;
        
        if(args->vextradata_size > 0){
             extraSize = args->vextradata_size + FF_INPUT_BUFFER_PADDING_SIZE;
             outCtx->extradata  = (uint8_t*)av_mallocz(extraSize); // this can be freed by avformat_free_context
             outCtx->extradata_size = args->vextradata_size;
             memcpy(outCtx->extradata, args->vextradata, outCtx->extradata_size);
        }

    }
    
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
    {
        outCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
    }


}

/* add a video output stream */
static AVStream * _add_video_stream(AVFormatContext *oc, int codec_id,
                                Lsp_args * args)
{
    AVCodecContext *c;
    AVStream *st;

    st = _av_new_stream_my(oc, oc->nb_streams);
    //st = av_new_stream(oc, oc->nb_streams); 
    if (!st) {
        LOG_ERROR("Could not alloc stream\n");
        exit(1);
    }
    //avcodec_get_context_defaults2(st->codec, AVMEDIA_TYPE_VIDEO);
    avcodec_get_context_defaults3(st->codec, NULL);

    c = st->codec;
    c->codec_id = codec_id;
    c->codec_type = AVMEDIA_TYPE_VIDEO;

    /* put sample parameters */
    c->bit_rate = args->vbit_rate;
    /* resolution must be a multiple of two */
    c->width = args->o_width;
    c->height = args->o_height;
    /* time base: this is the fundamental unit of time (in seconds) in terms
       of which frame timestamps are represented. for fixed-fps content,
       timebase should be 1/framerate and timestamp increments should be
       identically 1. */
    c->time_base.den = args->o_frame_rate;
    c->time_base.num = 1;
    c->gop_size = args->gop; /* emit one intra frame every twelve frames at most */

    c->max_b_frames = 2;
    c->pix_fmt = args->i_pix_fmt;
    
    //st->stream_copy = 1;
    
    // some formats want stream headers to be separate
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    c->thread_count = 1;

    //avcodec_thread_init(st->codec, c->thread_count);


    return st;
}

static AVStream *_add_audio_stream(AVFormatContext *oc, int codec_id,
                                  Lsp_args * args)
{
    AVCodecContext *c;
    AVStream *st;

    st = _av_new_stream_my(oc, oc->nb_streams);
    if (!st) {
        LOG_ERROR("Could not alloc stream\n");
        exit(1);
    }
    //avcodec_get_context_defaults2(st->codec, CODEC_TYPE_AUDIO);
    avcodec_get_context_defaults3(st->codec, NULL);

    c = st->codec;
    c->codec_id = codec_id;
    c->codec_type = AVMEDIA_TYPE_AUDIO; 


    /* put sample parameters */
    c->sample_fmt = AV_SAMPLE_FMT_S16;
    c->bit_rate = args->abit_rate;
    c->sample_rate = args->o_sample_rate;
    c->channels = 2;
    c->time_base.num = 1;
    c->time_base.den = c->sample_rate;
    
    //st->stream_copy = 1;

    // some formats want stream headers to be separate
    if(oc->oformat->flags & AVFMT_GLOBALHEADER)
        c->flags |= CODEC_FLAG_GLOBAL_HEADER;

    return st;
}


static int _create_output_ctx(struct Lsp_object * object)
{
    AVOutputFormat *fmt = NULL;
    AVFormatContext *oc = NULL;
    AVStream *audio_st, *video_st;
    int video_stream_idx, audio_stream_idx;
    int i;

    char * path = NULL;
    AVDictionary *opts = NULL;

    path = object->args.stream_out_path;
    LOG_DEBUG("path : %s\n", path);

    //avformat_alloc_output_context2(&oc, NULL, NULL, object->args.stream_out_path);
    avformat_alloc_output_context2(&oc, NULL, "flv", object->args.stream_out_path);
    if (!oc) {
        LOG_ERROR("Could not create output context\n");
        return ERROR_CODE_OUT_CTX_CREATE_ERROR;
    }
    LOG_DEBUG("avformat_alloc_output_context2 ok.\n");
    //oc->max_delay = 500000;
    //oc->flags |= AVFMT_FLAG_NONBLOCK;

    oc->interrupt_callback.callback = _lsp_interrupt_cb;
    oc->interrupt_callback.opaque = object;

    object->start_time = av_gettime();
    LOG_DEBUG("start_time : %lld\n", object->start_time);

    av_dict_set(&opts, "timeout", "5", 0); // in secs

    fmt = oc->oformat;

    /* open the output file, if needed */
    if (!(fmt->flags & AVFMT_NOFILE)) {
        //if (avio_open(&oc->pb, path, AVIO_FLAG_WRITE|AVIO_FLAG_NONBLOCK) < 0) {
        //    ERR("Could not open '%s'\n", oc->filename);
        //    free(oc);  // need free 
        //    return -1;
        //}
        if (avio_open2(&oc->pb, path, AVIO_FLAG_WRITE, &oc->interrupt_callback, NULL) < 0) {
            LOG_ERROR("Could not open '%s'\n", oc->filename);
            free(oc);  // need free 
            return ERROR_CODE_OUT_OPEN_FILE_ERROR;
        } 
        LOG_DEBUG("avio_open2(%s) ok.\n", path);
    }


    /* add the audio and video streams using the default format codecs
       and initialize the codecs */
    video_st = NULL;
    audio_st = NULL;
    if ((fmt->video_codec != AV_CODEC_ID_NONE) && object->args.have_video_stream) {
        video_st = _add_video_stream(oc, AV_CODEC_ID_H264, &object->args);
    }
    LOG_DEBUG("_add_video_stream over.\n");

    if ((fmt->audio_codec != AV_CODEC_ID_NONE) && object->args.have_audio_stream) {
        audio_st = _add_audio_stream(oc, AV_CODEC_ID_AAC, &object->args);
    }
    LOG_DEBUG("_add_audio_stream over.\n");

    av_dump_format(oc, 0, oc->filename, 1);
    LOG_DEBUG("av_dump_format filename(%s) over.\n", oc->filename);

    // Find audio/video stream.
    video_stream_idx = audio_stream_idx = -1;
    for (i = 0; i < oc->nb_streams; i++) {
        if (video_stream_idx == -1 && oc->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_idx = i;
            LOG_DEBUG("video_stream_idx=%d.\n", video_stream_idx);
            _set_stream_params(oc, i, &object->args);
        }
        if (audio_stream_idx == -1 && oc->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_idx = i;
            LOG_DEBUG("audio_stream_idx=%d.\n", audio_stream_idx);
            _set_stream_params(oc, i, &object->args);
        }
    }

    /* write the stream header, if any */
    if(avformat_write_header(oc, NULL)){
        LOG_ERROR("av_write_header error!\n");
        return ERROR_CODE_OUT_CTX_CREATE_ERROR;
    }
    LOG_DEBUG("avformat_write_header over.\n");

    object->outctx = oc;
    object->out_video_stream_index = video_stream_idx;
    object->out_audio_stream_index = audio_stream_idx;


    return 0;

}



AVPacket * _pack_enc_video_pkt(struct Lsp_object * handle, Lsp_data * data)
{
    if(data == NULL)
        return NULL;

    AVPacket *pkt = NULL;
    
    // malloc a pkt to fill encoded video
    pkt = malloc(sizeof(AVPacket));
    if (pkt == NULL) {
        LOG_ERROR("malloc failed for video pkt\n");
        return NULL;
    }
    av_init_packet(pkt);
    pkt->destruct = av_destruct_packet;
    pkt->data     = av_mallocz(data->size);
    if(pkt->data == NULL){
        LOG_ERROR("malloc failed for pkt->data\n");
        return NULL;
    }
    pkt->size = data->size;
    
    // filling data
    memcpy(pkt->data, data->data, data->size);
    
    // the key frame if need be set 
    if (data->is_key) {
        pkt->flags |= AV_PKT_FLAG_KEY;
    }

    // rebuild timestamp
    data->pts &= 0xffffffffffff;
    data->dts &= 0xffffffffffff;
    pkt->pts = av_rescale(data->pts, TIME_BASE, data->timebase);
    pkt->dts = av_rescale(data->dts, TIME_BASE, data->timebase);


    // set stream index, need fix
    if(handle->out_video_stream_index >= 0)
        pkt->stream_index = handle->out_video_stream_index;


    
    return pkt;
    
}


AVPacket * _pack_enc_audio_pkt(struct Lsp_object * handle, Lsp_data * data)
{
    if(data == NULL)
        return NULL;

    AVPacket *pkt = NULL;
    
    // malloc a pkt to fill encoded data
    pkt = malloc(sizeof(AVPacket));
    if (pkt == NULL) {
        LOG_ERROR("malloc failed for audio pkt\n");
        return NULL;
    }
    av_init_packet(pkt);
    pkt->destruct = av_destruct_packet;
    pkt->data     = av_mallocz(data->size);
    if(pkt->data == NULL){
        LOG_ERROR("malloc failed for pkt->data\n");
        return NULL;
    }
    pkt->size = data->size;
    
    // filling data
    memcpy(pkt->data, data->data, data->size);
    
    // the key frame if need be set 
    if (data->is_key) {
        pkt->flags |= AV_PKT_FLAG_KEY;
    }
    
    // rebuild timestamp
    pkt->pts = av_rescale(data->pts, TIME_BASE, data->timebase);
    pkt->dts = pkt->pts;
    
    // set stream index, need fix
    if(handle->out_audio_stream_index >= 0)
        pkt->stream_index = handle->out_audio_stream_index;

    return pkt;
    
}

static int _rebuild_timestamp(struct Lsp_object * handle, AVPacket *pkt)
{
    if(handle == NULL || pkt == NULL){
        return ERROR_CODE_NULL_POINTER;
    }
    int ret = 0;

    int timebase = 0;

    timebase = TIME_BASE;
    
    int64_t     dt;  // through framerate calculate dt
    int64_t     dt_pts_dts;

    //pkt->pts = pkt->pts & 0xffffffff;
    //pkt->dts = pkt->dts & 0xffffffff;
    
    // get the val = pts - dts, val will used to rebuild pts by rebuilt dts
    dt_pts_dts = pkt->pts - pkt->dts;

    if(handle->args.o_frame_rate){
        dt = 2*(timebase/handle->args.o_frame_rate);
    }else{
        dt = 2*(timebase/25);
    }
    
    //printf("timebase:%d, dt:%lld.\n",timebase,dt);
    
    if(handle->args.have_video_stream > 0){
        
        if(pkt->stream_index == handle->out_video_stream_index ){
        //video
            //printf("video,first_vdts:%lld, vdts:%lld, pts:%lld, dts:%lld.\n",handle->in_first_vdts,handle->vdts,pkt->pts,pkt->dts);
            if(handle->in_first_vdts <= 0){
                
                handle->in_first_vdts = pkt->dts;  // save the first dts
                handle->vdts = 0;

            }else{
                handle->vdts = pkt->dts - handle->in_first_vdts;
            }

            pkt->dts = handle->vdts;
            pkt->pts = pkt->dts + dt_pts_dts + dt;
            
        }
        
        if(pkt->stream_index == handle->out_audio_stream_index ){
        // audio
            if(handle->in_first_vdts <= 0){
                // if video dts not rebuild
                LOG_DEBUG("handle->in_first_vdts <= 0\n");
                return -1;
            }

            if(handle->in_first_adts == 0){
                if(pkt->dts <= handle->in_first_vdts){
                    LOG_DEBUG("audio dts > first vdts\n");
                    return -1;
                }else{
                    handle->in_first_adts = pkt->dts;
                    handle->adts = 0;
                }
            }else{
                handle->adts = pkt->dts - handle->in_first_adts;
                
            }

            pkt->dts = handle->adts;
            pkt->pts = pkt->dts + dt_pts_dts + dt;

        }
    }
    else{
        if(pkt->stream_index == handle->out_audio_stream_index ){

        //audio
            if(handle->in_first_adts == 0){
                handle->in_first_adts = pkt->dts;
                handle->adts = 0;
                
            }else{
                handle->adts = pkt->dts - handle->in_first_adts;
                
            }
            
            pkt->dts = handle->adts;
            pkt->pts = pkt->dts + dt_pts_dts + dt;

        }

    }
    return ret;
}











/**
 * Create the Lsp object.
 * @return          not NULL if OK,  NULL if error
 */
Lsp_handle Lsp_create(void)
{
    Lsp_handle ptr = NULL;

    ptr = (Lsp_handle *)malloc(sizeof(struct Lsp_object));
    memset(ptr, 0, sizeof(struct Lsp_object));

    return ptr;
}

/**
 * Init the Lsp object by Lsp_args.
 * @param obj       the pointer point to Lsp_obj which will be inited
 * @param args      the pointer point to Lsp_args that contained params for init
 * @return          0 if OK,  <0 if some error
 */
int Lsp_init(Lsp_handle handle, struct Lsp_args * args)
{
    int ret;

    // check input params
    if(handle == NULL){
        LOG_ERROR("handle == NULL\n");
        return ERROR_CODE_NULL_POINTER;
    }
    if(args == NULL){
        LOG_ERROR("args == NULL\n");
        return ERROR_CODE_NULL_POINTER;
    }

    struct Lsp_object * object = (struct Lsp_object *)handle;

    // memeset the object
    memset(object, 0, sizeof(struct Lsp_object));

    // check input args and update to xargs
    ret = _check_iargs_and_update_args(&object->args, args);
    if(ret < 0){
        return ret;
    }


    av_register_all();
    
    // create output 
    ret = _create_output_ctx(object);
    if(ret < 0){
        return ret;
    }

    return ERROR_CODE_OK;
}

int Lsp_push(Lsp_handle handle, struct Lsp_data * data)
{
    // check input params
    if(handle == NULL){
        LOG_ERROR("handle == NULL\n");
        return ERROR_CODE_NULL_POINTER;
    }
    if(data == NULL){
        LOG_ERROR("data == NULL\n");
        return ERROR_CODE_NULL_POINTER;
    }


    struct Lsp_object * object = (struct Lsp_object *)handle;

    int ret = 0;
    AVPacket * pkt = NULL;


    LOG_DEBUG("data type:%d, size:%d, pts:%lld\n", data->type, data->size, data->pts);

    // check the data, and pack it 
    if(data->type == DATA_TYPE_ENC_VIDEO && object->args.have_video_stream){
        
        pkt = _pack_enc_video_pkt(object, data);
        if(pkt == NULL){
            LOG_ERROR("_pack_enc_video_pkt NULL\n");
            return ERROR_CODE_PACK_ENC_VIDEO_ERROR;
        }
    }
    else if(data->type == DATA_TYPE_ENC_AUDIO && object->args.have_audio_stream){
        pkt = _pack_enc_audio_pkt(object, data);
        if(pkt == NULL){
            LOG_ERROR("_pack_enc_audio_pkt error\n");
            return ERROR_CODE_PACK_ENC_AUDIO_ERROR;
        }

    }
    else{
        LOG_ERROR("date type error\n");
        return ERROR_CODE_DATA_TYPE_ERROR;
    }


    // rebuild timestamp
    if(pkt){

        LOG_DEBUG("before rebuild, type: %d, timebase:%lld, index:%d, pts:%lld,dts:%lld\n", 
            data->type, data->timebase, pkt->stream_index, pkt->pts, pkt->dts);
                
        if(_rebuild_timestamp(object, pkt) < 0){
        
            av_free_packet(pkt);
            av_free(pkt);
            LOG_ERROR("_rebuild_timestamp error\n");
            return ret;
        }

        LOG_DEBUG("after rebuild, type: %d, timebase:%lld, index:%d, pts:%lld,dts:%lld\n", 
            data->type, data->timebase, pkt->stream_index, pkt->pts, pkt->dts);

    }else{
        LOG_DEBUG("pkt NULL, type:%d, ts:%lld\n", data->type, data->pts);
    }

    object->start_time = av_gettime();
    
    if(object->outctx != NULL && pkt){
        LOG_DEBUG("write a frame to out, type: %d, pts:%lld, dts:%lld, vcount:%lld, acont:%lld.\n", data->type,pkt->pts, pkt->dts,
            object->outctx->streams[object->out_video_stream_index]->nb_frames,
            object->outctx->streams[object->out_audio_stream_index]->nb_frames);
        if (av_interleaved_write_frame(object->outctx, pkt) < 0) {
            LOG_ERROR("error av_write_frame.\n");
            return ERROR_CODE_OUT_STREAM_ERROR;
        }

        object->video_frame_cnt = object->outctx->streams[object->out_video_stream_index]->nb_frames;
        object->audio_sample_cnt = object->outctx->streams[object->out_audio_stream_index]->nb_frames;
    }
        
    av_free_packet(pkt);
    av_free(pkt);

    return ERROR_CODE_OK;
}
