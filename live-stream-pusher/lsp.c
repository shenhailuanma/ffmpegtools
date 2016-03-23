#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>

#include "lsp.h"

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




static int _check_iargs_and_update_args(struct Lsp_args * xargs, struct Lsp_args * iargs)
{
    // don't check args pointer, trust it

    if(strlen(stream_src_path) <= 0){
        return ERROR_CODE_NO_INPUT;
    }

    xargs->have_audio_stream = iargs->have_audio_stream;
    xargs->have_video_stream = iargs->have_video_stream;
    xargs->have_file_stream  = iargs->have_file_stream;
    xargs->have_rtmp_stream  = iargs->have_rtmp_stream;


    if(xargs->have_audio_stream <= 0 && xargs->have_video_stream <= 0){
        return ERROR_CODE_NO_INPUT;
    }

    if(xargs->have_file_stream <= 0 && xargs->have_rtmp_stream <= 0){
        return ERROR_CODE_NO_OUTPUT;
    }

    // check raw input params
    if(xargs->have_video_stream){
        if(VAL_IF_IN_RANGE(iargs->i_width, 16, 1920)){
            xargs->i_width = iargs->i_width; 
        }else{
            return ERROR_CODE_VIDEO_WIDTH_ERROR;
        }
        
        if(VAL_IF_IN_RANGE(iargs->i_height, 16, 1080)){
            xargs->i_height = iargs->i_height; 
        }else{
            return ERROR_CODE_VIDEO_HEIGHT_ERROR;
        }   

        xargs->o_pix_fmt = xargs->i_pix_fmt = AV_PIX_FMT_YUV420P; // now input raw video only support this fmt
            
        if(VAL_IF_IN_RANGE(iargs->i_frame_rate, 1, 30)){
            xargs->i_frame_rate = iargs->i_frame_rate; 
        }else{
            return ERROR_CODE_VIDEO_FRAME_RATE_ERROR;
        }
        
        if(VAL_IF_IN_RANGE(iargs->coder_type, VIDEO_CODER_TYPE_CAVLC, VIDEO_CODER_TYPE_CABAC)){
            xargs->coder_type = iargs->coder_type; 
        }else{
            return ERROR_CODE_VIDEO_FRAME_RATE_ERROR;
        }

    }

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
        xargs->o_width = iargs->i_width;
    }

    if(VAL_IF_IN_RANGE(iargs->o_height, 144, 1080)){
        xargs->o_height = iargs->o_height; 
    }else{
        xargs->o_height = iargs->i_height;
    }

    if(VAL_IF_IN_RANGE(iargs->o_frame_rate, 1, 30)){
        xargs->o_frame_rate = iargs->o_frame_rate; 
    }else{
        xargs->o_frame_rate = iargs->i_frame_rate;
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

    if(strlen(iargs->stream_out_path))
        strcpy(xargs->stream_out_path, iargs->stream_out_path);

    if(strlen(iargs->stream_save_path))
        strcpy(xargs->stream_save_path, iargs->stream_save_path);


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



/**
 * Init the Lsp object by Lsp_args.
 * @param obj       the pointer point to Lsp_obj which will be inited
 * @param args      the pointer point to Lsp_args that contained params for init
 * @return          0 if OK,  <0 if some error
 */
int Lsp_init(struct Lsp_obj * obj, struct Lsp_args * args)
{
    int ret;

    // check input params
    if(obj == NULL){
        LOG_ERROR("obj == NULL\n");
        return ERROR_CODE_NULL_POINTER;
    }
    if(args == NULL){
        LOG_ERROR("args == NULL\n");
        return ERROR_CODE_NULL_POINTER;
    }

    // memeset the object
    memset(obj, 0, sizeof(struct Lsp_obj));

    // check input args and update to xargs
    ret = _check_iargs_and_update_args(&obj->args, args);
    if(ret < 0){
        return ret;
    }


    av_register_all();
    // get src media info

    
    // create output 

    return ERROR_CODE_OK;
}


