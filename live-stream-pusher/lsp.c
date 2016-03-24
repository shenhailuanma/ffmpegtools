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
    AVFormatContext *savectx;  // output ctx
    int out_video_stream_index;
    int out_audio_stream_index;
    int save_video_stream_index;
    int save_audio_stream_index;
    
    Lsp_args args;
    
    // for rebuild timestamp
    int64_t in_first_vdts;  // input first video dts
    int64_t vdts;           // rebuilt video dts
    int64_t in_first_adts;  // input first audio dts
    int64_t adts;

    int64_t start_time;
    
};



static int _check_iargs_and_update_args(struct Lsp_args * xargs, struct Lsp_args * iargs)
{
    // don't check args pointer, trust it

    if(strlen(iargs->stream_src_path) <= 0){
        return ERROR_CODE_NO_INPUT;
    }

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

    if(VAL_IF_IN_RANGE(iargs->abit_rate, 32, 196)){
        xargs->abit_rate = iargs->abit_rate; 
    }else{
        xargs->abit_rate = 48;
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

    if(strlen(iargs->stream_out_path))
        strcpy(xargs->stream_out_path, iargs->stream_out_path);

    return ERROR_CODE_OK;
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
    // get src media info

    
    // create output 

    return ERROR_CODE_OK;
}


