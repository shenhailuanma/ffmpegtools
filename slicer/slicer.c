

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>


#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"

#include "libavcodec/get_bits.h"
#include "libavcodec/h264.h"


#include "libavutil/timestamp.h"
#include "libavutil/opt.h"


/****  Defines *****/
static int Glb_Log_Level = 3;
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

#define SLICER_GROUP_TYPE_NORMAL            0
#define SLICER_GROUP_TYPE_START             1
#define SLICER_GROUP_TYPE_END               2 


typedef struct list_head{
  struct list_head *next;
  struct list_head *prev;
} list_t;

/* Initialize a new list head.  */
#define INIT_LIST_HEAD(ptr)  (ptr)->next = (ptr)->prev = (ptr)

#define container_of(ptr, type, member) \
  (type *)((char *)ptr - \
  (unsigned long)(&(((type *)0)->member)))

#define list_entry(ptr, type, member) \
  container_of(ptr, type, member)
    
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

    /* the slice start time that use common timebase */
    int64_t slice_start_time;

    /* the slice end time that use common timebase */
    int64_t slice_end_time;

    /* the video Key frame timestamp which just before the user set slice start time */
    int64_t key_frame_timestamp_before_start_time;

    /* the video Key frame timestamp which just before the user set slice end time */
    int64_t key_frame_timestamp_before_end_time;

    /* packets queue, used in mode:'encode copy encode' */
    struct list_head packets_queue_head;

    /* packets group count, used in mode:'encode copy encode' */
    int packets_group_count;

    int first_video_stream_index;

    int64_t first_video_frame_pts;
    int64_t first_video_frame_dts;

    int64_t first_video_frame_pts_input_timebase;
    int64_t first_video_frame_dts_input_timebase;

    int64_t dt_timestamp_input_timebase;

} Slicer_t;

typedef struct Slicer_Packet_t{
    struct list_head head;
    AVPacket packet;
    int number;
}Slicer_Packet_t;

typedef struct Slicer_timestamp_t{
    struct list_head head;
    int64_t pts;
    int64_t dts;
}Slicer_timestamp_t;



/******  Functions ******/
/* Add new element at the head of the list.  */
static inline void list_add(list_t *newp, list_t *head)
{
    head->next->prev = newp;
    newp->next = head->next;
    newp->prev = head;
    head->next = newp;
}


/* Add new element at the tail of the list.  */
static inline void list_add_tail(list_t *newp, list_t *head)
{
    head->prev->next = newp;
    newp->next = head;
    newp->prev = head->prev;
    head->prev = newp;
}


/* Remove element from list.  */
static inline void list_del(list_t *elem)
{
    elem->next->prev = elem->prev;
    elem->prev->next = elem->next;
}

static void print_hex(char * phex, int size)
{
    if(phex == NULL)
        return;

    int i = 0;
    while(i < size){
        printf("%02x ", (unsigned char)phex[i]);
        i++;
    }

    printf("\n");
}


static void Slicer_params_default(Slicer_Params_t * params)
{
    memset(params, 0, sizeof(Slicer_Params_t));
    params->slice_mode = SLICER_MODE_COPY_ONLY;
    params->log_level = 4;
}

static void Slicer_print_sps(SPS *sps)
{
    LOG_DEBUG("SPS: sps_id=%u.\n", sps->sps_id);
    LOG_DEBUG("SPS: profile_idc=%d.\n", sps->profile_idc);
    LOG_DEBUG("SPS: level_idc=%d.\n", sps->level_idc);
    LOG_DEBUG("SPS: chroma_format_idc=%d.\n", sps->chroma_format_idc);
    LOG_DEBUG("SPS: transform_bypass=%d.\n", sps->transform_bypass);
    LOG_DEBUG("SPS: log2_max_frame_num=%d.\n", sps->log2_max_frame_num);
    LOG_DEBUG("SPS: poc_type=%d.\n", sps->poc_type);
    LOG_DEBUG("SPS: log2_max_poc_lsb=%d.\n", sps->log2_max_poc_lsb);
    LOG_DEBUG("SPS: delta_pic_order_always_zero_flag=%d.\n", sps->delta_pic_order_always_zero_flag);
    LOG_DEBUG("SPS: offset_for_non_ref_pic=%d.\n", sps->offset_for_non_ref_pic);
    LOG_DEBUG("SPS: offset_for_top_to_bottom_field=%d.\n", sps->offset_for_top_to_bottom_field);

    LOG_DEBUG("SPS: poc_cycle_length=%d.\n", sps->poc_cycle_length);
    LOG_DEBUG("SPS: ref_frame_count=%d.\n", sps->ref_frame_count);
    LOG_DEBUG("SPS: gaps_in_frame_num_allowed_flag=%d.\n", sps->gaps_in_frame_num_allowed_flag);
    LOG_DEBUG("SPS: mb_width=%d.\n", sps->mb_width);
    LOG_DEBUG("SPS: mb_height=%d.\n", sps->mb_height);
    LOG_DEBUG("SPS: frame_mbs_only_flag=%d.\n", sps->frame_mbs_only_flag);

    LOG_DEBUG("SPS: mb_aff=%d.\n", sps->mb_aff);
    LOG_DEBUG("SPS: direct_8x8_inference_flag=%d.\n", sps->direct_8x8_inference_flag);
    LOG_DEBUG("SPS: crop=%d.\n", sps->crop);
    LOG_DEBUG("SPS: crop_left=%u.\n", sps->crop_left);
    LOG_DEBUG("SPS: crop_right=%u.\n", sps->crop_right);
    LOG_DEBUG("SPS: crop_top=%u.\n", sps->crop_top);
    LOG_DEBUG("SPS: crop_bottom=%u.\n", sps->crop_bottom);

    LOG_DEBUG("SPS: vui_parameters_present_flag=%d.\n", sps->vui_parameters_present_flag);
    LOG_DEBUG("SPS: sar.num=%d.\n", sps->sar.num);
    LOG_DEBUG("SPS: sar.den=%d.\n", sps->sar.den);
    LOG_DEBUG("SPS: video_signal_type_present_flag=%d.\n", sps->video_signal_type_present_flag);
    LOG_DEBUG("SPS: full_range=%d.\n", sps->full_range);
    LOG_DEBUG("SPS: colour_description_present_flag=%d.\n", sps->colour_description_present_flag);
    LOG_DEBUG("SPS: color_primaries=%d.\n", sps->color_primaries);
    LOG_DEBUG("SPS: color_trc=%d.\n", sps->color_trc);
    LOG_DEBUG("SPS: colorspace=%d.\n", sps->colorspace);
    LOG_DEBUG("SPS: timing_info_present_flag=%d.\n", sps->timing_info_present_flag);


    LOG_DEBUG("SPS: num_units_in_tick=%u.\n", sps->num_units_in_tick);
    LOG_DEBUG("SPS: time_scale=%u.\n", sps->time_scale);
    LOG_DEBUG("SPS: fixed_frame_rate_flag=%d.\n", sps->fixed_frame_rate_flag);

    // offset_for_ref_frame 
    LOG_DEBUG("SPS: offset_for_ref_frame=");
    print_hex(sps->offset_for_ref_frame, 512);

    LOG_DEBUG("SPS: bitstream_restriction_flag=%d.\n", sps->bitstream_restriction_flag);
    LOG_DEBUG("SPS: num_reorder_frames=%d.\n", sps->num_reorder_frames);
    LOG_DEBUG("SPS: scaling_matrix_present=%d.\n", sps->scaling_matrix_present);

    // scaling_matrix4
    LOG_DEBUG("SPS: scaling_matrix4=");
    print_hex(sps->scaling_matrix4, 96);
    // scaling_matrix8
    LOG_DEBUG("SPS: scaling_matrix8=");
    print_hex(sps->scaling_matrix8, 384);

    LOG_DEBUG("SPS: nal_hrd_parameters_present_flag=%d.\n", sps->nal_hrd_parameters_present_flag);
    LOG_DEBUG("SPS: vcl_hrd_parameters_present_flag=%d.\n", sps->vcl_hrd_parameters_present_flag);
    LOG_DEBUG("SPS: pic_struct_present_flag=%d.\n", sps->pic_struct_present_flag);
    LOG_DEBUG("SPS: time_offset_length=%d.\n", sps->time_offset_length);

    LOG_DEBUG("SPS: cpb_cnt=%d.\n", sps->cpb_cnt);
    LOG_DEBUG("SPS: initial_cpb_removal_delay_length=%d.\n", sps->initial_cpb_removal_delay_length);
    LOG_DEBUG("SPS: cpb_removal_delay_length=%d.\n", sps->cpb_removal_delay_length);
    LOG_DEBUG("SPS: dpb_output_delay_length=%d.\n", sps->dpb_output_delay_length);

    LOG_DEBUG("SPS: bit_depth_luma=%d.\n", sps->bit_depth_luma);
    LOG_DEBUG("SPS: bit_depth_chroma=%d.\n", sps->bit_depth_chroma);
    LOG_DEBUG("SPS: residual_color_transform_flag=%d.\n", sps->residual_color_transform_flag);
    LOG_DEBUG("SPS: constraint_set_flags=%d.\n", sps->constraint_set_flags);
    LOG_DEBUG("SPS: new=%d.\n", sps->new);
}

static void Slicer_print_pps(PPS *pps)
{
    LOG_DEBUG("PPS: sps_id=%u.\n", pps->sps_id);
    LOG_DEBUG("PPS: cabac=%d.\n", pps->cabac);
    LOG_DEBUG("PPS: pic_order_present=%d.\n", pps->pic_order_present);
    LOG_DEBUG("PPS: slice_group_count=%d.\n", pps->slice_group_count);
    LOG_DEBUG("PPS: mb_slice_group_map_type=%d.\n", pps->mb_slice_group_map_type);

    LOG_DEBUG("PPS: ref_count[0]=%u.\n", pps->ref_count[0]);
    LOG_DEBUG("PPS: ref_count[1]=%u.\n", pps->ref_count[1]);
    LOG_DEBUG("PPS: weighted_pred=%d.\n", pps->weighted_pred);
    LOG_DEBUG("PPS: weighted_bipred_idc=%d.\n", pps->weighted_bipred_idc);
    LOG_DEBUG("PPS: init_qp=%d.\n", pps->init_qp);
    LOG_DEBUG("PPS: init_qs=%d.\n", pps->init_qs);

    LOG_DEBUG("PPS: chroma_qp_index_offset[0]=%d.\n", pps->chroma_qp_index_offset[0]);
    LOG_DEBUG("PPS: chroma_qp_index_offset[1]=%d.\n", pps->chroma_qp_index_offset[1]);
    LOG_DEBUG("PPS: deblocking_filter_parameters_present=%d.\n", pps->deblocking_filter_parameters_present);
    LOG_DEBUG("PPS: constrained_intra_pred=%d.\n", pps->constrained_intra_pred);
    LOG_DEBUG("PPS: redundant_pic_cnt_present=%d.\n", pps->redundant_pic_cnt_present);

    LOG_DEBUG("PPS: transform_8x8_mode=%d.\n", pps->transform_8x8_mode);

    // scaling_matrix4
    LOG_DEBUG("PPS: scaling_matrix4=");
    print_hex(pps->scaling_matrix4, 96);
    // scaling_matrix8
    LOG_DEBUG("PPS: scaling_matrix8=");
    print_hex(pps->scaling_matrix8, 384);

    // chroma_qp_table
    LOG_DEBUG("PPS: chroma_qp_table=");
    print_hex(pps->chroma_qp_table, 176);


    LOG_DEBUG("PPS: chroma_qp_diff=%d.\n", pps->chroma_qp_diff);

}


int Slicer_decode_h264_extradata(Slicer_t *obj, uint8_t *extradata, int extradata_size)
{
    // check params
    //if(sps == NULL && pps == NULL){
    //    LOG_WARN("sps == NULL && pps == NULL, no need to decode, return.\n");
    //    return 0;
    //}

    if(extradata == NULL){
        LOG_ERROR("extradata == NULL, return.\n");
        return 0;
    }

    if(extradata_size <= 0){
        LOG_ERROR("extradata_size <= 0, return.\n");
        return 0;
    }

    int ret;
    int i;
    int n;
    SPS *sps = NULL;
    PPS *pps = NULL;
    AVCodecParserContext * parser_ctx = NULL;
    H264Context * h264_ctx = NULL;


    // open input file, just get H264Context to parse extradata
    AVFormatContext * ctx = NULL;
    ret = avformat_open_input(&ctx, obj->params.input_url, NULL, NULL);
    if (ret < 0 || ctx == NULL) {
        LOG_ERROR("open stream: '%s', error code: %d \n", obj->params.input_url, ret);
        return -1;
    }

    ctx->ctx_flags = ctx->ctx_flags | AVFMTCTX_NOHEADER;
    for(i = 0; i < ctx->nb_streams; i++){
        if (ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {

            LOG_DEBUG("streams[%d] is video stream, parser_ctx=%ld, need_parsing=%d, to set need_parsing=AVSTREAM_PARSE_FULL_RAW.\n", 
                i, ctx->streams[i]->parser, ctx->streams[i]->need_parsing);
            ctx->streams[i]->need_parsing = AVSTREAM_PARSE_FULL_RAW;
        }
    }
    // to find the streams info
    ret = avformat_find_stream_info(ctx, NULL);
    if (ret < 0) {
        LOG_ERROR("could not find stream info.\n");
        return -1;
    }

    LOG_DEBUG("ctx->flags & AVFMT_FLAG_NOPARSE = %d.\n", ctx->flags&AVFMT_FLAG_NOPARSE);
    LOG_DEBUG("ctx->ctx_flags & AVFMTCTX_NOHEADER = %d.\n", ctx->ctx_flags&AVFMTCTX_NOHEADER);

    // 
    for(i = 0; ctx->nb_streams; i++){
        if(ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
            LOG_DEBUG("find streams[%d] is video stream, to parse sps pps.\n", i);

            parser_ctx = ctx->streams[i]->parser;
            LOG_DEBUG("parser_ctx=%ld, need_parsing=%d\n", parser_ctx, ctx->streams[i]->need_parsing);
            ctx->streams[i]->need_parsing = AVSTREAM_PARSE_FULL_RAW;

            if(parser_ctx){
                h264_ctx = parser_ctx->priv_data;
                LOG_DEBUG("h264_ctx=%ld, .\n", h264_ctx);

                ff_h264_decode_extradata(h264_ctx, extradata, extradata_size);
                LOG_DEBUG("\n");
                // get sps and pps
                for(n = 0; n < MAX_SPS_COUNT; n++){
                    sps = NULL;
                    sps = h264_ctx->sps_buffers[n];
                    //LOG_DEBUG("sps=%ld, n=%d\n", sps, n);
                    if (sps)
                        Slicer_print_sps(sps);
                }
                for(n = 0; n < MAX_PPS_COUNT; n++){
                    pps = NULL;
                    pps = h264_ctx->pps_buffers[n];
                    //LOG_DEBUG("sps=%ld, n=%d\n", sps, n);
                    if(pps)
                        Slicer_print_pps(pps);
                }
            }
            break;
        }else{
            LOG_DEBUG("find streams[%d] is not video stream, continue.\n", i);
            break;
        }
        
    }
    LOG_DEBUG("\n");
    // close file
    //avformat_close_input(&ctx);
    LOG_DEBUG("\n");
    return 0;
}



static int Slicer_open_input_media(Slicer_t *obj)
{
    if(obj == NULL){
        LOG_ERROR("Slicer_open_input_media input arg: obj is NULL\n");
        return -1;
    }

    int ret = 0;
    int i;

    obj->input_ctx = NULL;
    ret = avformat_open_input(&obj->input_ctx, obj->params.input_url, NULL, NULL);
    if (ret < 0 || obj->input_ctx == NULL) {
        LOG_ERROR("open stream: '%s', error code: %d \n", obj->params.input_url, ret);
        return -1;
    }

    // for test
    /*
    //obj->input_ctx->ctx_flags = obj->input_ctx->ctx_flags | AVFMTCTX_NOHEADER;
    //obj->input_ctx->max_analyze_duration = 500000000;
    //obj->input_ctx->debug |= FF_FDEBUG_TS;

    for(i = 0; i < obj->input_ctx->nb_streams; i++){
        if (obj->input_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {

            LOG_DEBUG("streams[%d] is video stream, parser_ctx=%ld, need_parsing=%d, to set need_parsing=AVSTREAM_PARSE_FULL_RAW.\n", 
                i, obj->input_ctx->streams[i]->parser, obj->input_ctx->streams[i]->need_parsing);
            obj->input_ctx->streams[i]->need_parsing = AVSTREAM_PARSE_FULL_RAW;
        }
    }
    */


    // to find the streams info
    ret = avformat_find_stream_info(obj->input_ctx, NULL);
    if (ret < 0) {
        LOG_ERROR("could not find stream info.\n");
        return -1;
    }



    // if slice mode need encode, should open the decoder and encoder
    if(obj->params.slice_mode == SLICER_MODE_ENCODE_ONLY || obj->params.slice_mode == SLICER_MODE_ENCODE_COPY_ENCODE){
        LOG_DEBUG("slice mode need encode, to open the video decoder.\n");

        // open the video decoder
        for(i = 0; i < obj->input_ctx->nb_streams; i++){
            

            if (obj->input_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
                LOG_DEBUG("find streams[%d] is video stream, need to open video codec.\n", i);
                // for test
                LOG_INFO(" streams[%d] extradata_size:%d.\n", i, obj->input_ctx->streams[i]->codec->extradata_size);
                print_hex(obj->input_ctx->streams[i]->codec->extradata, obj->input_ctx->streams[i]->codec->extradata_size);
                //Slicer_h264_decode_extradata(obj->input_ctx->streams[i]->codec->extradata, obj->input_ctx->streams[i]->codec->extradata_size);
                Slicer_decode_h264_extradata(obj, obj->input_ctx->streams[i]->codec->extradata, obj->input_ctx->streams[i]->codec->extradata_size);

                // open decoder 
                ret = avcodec_open2(obj->input_ctx->streams[i]->codec, avcodec_find_decoder(obj->input_ctx->streams[i]->codec->codec_id), NULL);
                if (ret < 0) {
                    LOG_ERROR("Failed to open decoder for stream #%d\n", i);
                    return ret;
                }
            }
        }


    }else{
        LOG_DEBUG("slice mode no need encode, just copy it.\n");
    }

    //obj->input_ctx->flags |= AVFMT_FLAG_GENPTS;
    av_dump_format(obj->input_ctx, 0, obj->input_ctx->filename, 0);



    return 0;
}




static void Slicer_parse_sps(uint8_t * data, int size)
{

}


static void Slicer_print_codec_context(AVCodecContext *ctx)
{

    printf("AVCodecContext:   id:%d\n", ctx->codec_id);
    printf("AVCodecContext:   name:%s\n", ctx->codec_name);
    printf("AVCodecContext:   codec:%d\n", ctx->codec);
    printf("AVCodecContext:   codec_type:%d\n", ctx->codec_type);
    printf("AVCodecContext:   codec_tag:%d\n", ctx->codec_tag);
    printf("AVCodecContext:   stream_codec_tag:%d\n", ctx->stream_codec_tag);
    printf("AVCodecContext:   av_class:%s\n", ctx->av_class->class_name);
    printf("AVCodecContext:   profile:%d\n", ctx->profile);
    printf("AVCodecContext:   bit_rate:%d\n", ctx->bit_rate);

    printf("AVCodecContext:   rc_override_count:%d\n", ctx->rc_override_count);
    printf("AVCodecContext:   rc_override:%d\n", ctx->rc_override);
    printf("AVCodecContext:   priv_data:%d\n", ctx->priv_data);
    printf("AVCodecContext:   rc_eq:%d\n", ctx->rc_eq);
    printf("AVCodecContext:   extradata:%d\n", ctx->extradata);
    printf("AVCodecContext:   intra_matrix:%d\n", ctx->intra_matrix);
    printf("AVCodecContext:   inter_matrix:%d\n", ctx->inter_matrix);
    printf("AVCodecContext:   subtitle_header:%d\n", ctx->subtitle_header);


    printf("AVCodecContext:  height = %d \n", ctx->width);
    printf("AVCodecContext:  width = %d \n", ctx->width);

    printf("AVCodecContext:  me_range = %d \n", ctx->me_range);
    printf("AVCodecContext:  me_subpel_quality = %d \n", ctx->me_subpel_quality);
    printf("AVCodecContext:  max_qdiff = %d \n", ctx->max_qdiff);
    printf("AVCodecContext:  qmin = %d \n", ctx->qmin);
    printf("AVCodecContext:  qmax = %d \n", ctx->qmax);
    printf("AVCodecContext:  qcompress = %f \n", ctx->qcompress);
    printf("AVCodecContext:  i_quant_factor = %f \n", ctx->i_quant_factor);
    printf("AVCodecContext:  b_quant_factor = %f \n", ctx->b_quant_factor);
    printf("AVCodecContext:  gop_size = %d \n", ctx->gop_size);
    printf("AVCodecContext:  coder_type = %d \n", ctx->coder_type);
    printf("AVCodecContext:  chromaoffset = %d \n", ctx->chromaoffset);
    printf("AVCodecContext:  pix_fmt = %d \n", ctx->pix_fmt);
    printf("AVCodecContext:  time_base.den = %d \n", ctx->time_base.den);
    printf("AVCodecContext:  time_base.num  = %d \n", ctx->time_base.num );
    printf("AVCodecContext:  ticks_per_frame = %d \n", ctx->ticks_per_frame );
    av_opt_show2(ctx->priv_data, NULL, 0, 0);
}



static void Slicer_set_video_encoder_context(Slicer_t *obj, AVCodecContext *dest, const AVCodecContext *src, AVDictionary **opts)
{
    //Slicer_parse_sps(src->extradata, src->extradata_size);

    dest->height = src->height;
    dest->width = src->width;
    dest->sample_aspect_ratio = src->sample_aspect_ratio;
    //dest->pix_fmt = src->pix_fmt;
    dest->pix_fmt = AV_PIX_FMT_YUV420P;
    dest->time_base.den = src->time_base.den / 2;
    dest->time_base.num = src->time_base.num;


    dest->me_range = 16;
    dest->max_qdiff = 4;
    dest->gop_size = 250;
    dest->qmin = 0;
    dest->qmax = 40;
    dest->qcompress = 0.6;
    dest->i_quant_factor = 1.4;
    dest->b_quant_factor = 1.3;


    // 
    dest->priv_data = NULL;
    dest->codec = NULL;
    dest->codec_tag = 0;

    dest->coder_type = 1;  // FIXME: just add for test
    // set params for can generate same sps/pps

    // FIXME:  just add for test, need auto check by sps pps
    av_dict_set(opts, "weightp", "0", 0);
    dest->chromaoffset = 0; 
    av_dict_set(opts, "chroma-qp-offset", "0", 0);
    dest->me_subpel_quality = 5; 

    //av_dict_set(opts, "x264-params", "sps-id=6", 0);
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
    //avformat_alloc_output_context2(&obj->output_ctx, NULL, "mp4", obj->params.output_url);
    if (!obj->output_ctx) {
        LOG_ERROR("Could not create output context\n");
        return -1;
    }


    int i;
    AVStream *in_stream = NULL;
    AVStream *out_stream = NULL;
    for (i = 0; i < obj->input_ctx->nb_streams; i++) {
        in_stream  = obj->input_ctx->streams[i];

        // if slice mode need encode, should open the video encoder
        if((in_stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) && 
            (obj->params.slice_mode == SLICER_MODE_ENCODE_ONLY || obj->params.slice_mode == SLICER_MODE_ENCODE_COPY_ENCODE)){
            
            LOG_DEBUG("slice mode need encode, to open the encoder.\n");


            // find the encoder
            AVCodec *encoder;
            encoder = avcodec_find_encoder(in_stream->codec->codec_id);
            if (!encoder) {
                LOG_ERROR("Necessary encoder not found\n");
                return -1;
            }
            LOG_DEBUG("open the encoder %d ok.\n", in_stream->codec->codec_id);

            out_stream = avformat_new_stream(obj->output_ctx, encoder);
            //out_stream = avformat_new_stream(obj->output_ctx, NULL);

            LOG_DEBUG("add new stream %d ok.\n", out_stream);

            ret = 0;
            ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
            if (ret < 0) {
                LOG_ERROR("Failed to copy context from input to output stream codec context\n");
                return -1;
            }

            AVDictionary *opts = NULL;
            // set the video encoder context for open the encoder
            Slicer_set_video_encoder_context(obj, out_stream->codec, in_stream->codec, &opts);


            if (obj->output_ctx->oformat->flags & AVFMT_GLOBALHEADER){
                LOG_DEBUG("AVFMT_GLOBALHEADER is True.\n");
                out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            }

            // print for test
            LOG_DEBUG("Slicer_print_codec_context in stream context.\n");
            Slicer_print_codec_context(in_stream->codec);
            LOG_DEBUG("Slicer_print_codec_context out stream context.\n");
            Slicer_print_codec_context(out_stream->codec);

            ret = avcodec_open2(out_stream->codec, encoder, &opts);
            if (ret < 0) {
                LOG_ERROR("Cannot open video encoder for stream #%u\n", i);
                return ret;
            }
            av_dict_free(&opts);
            LOG_DEBUG("avcodec_open2 ok.\n");

            LOG_INFO("[OUT] streams[%d] extradata_size:%d.\n", i, out_stream->codec->extradata_size);
            print_hex(out_stream->codec->extradata, out_stream->codec->extradata_size);

            Slicer_decode_h264_extradata(obj, out_stream->codec->extradata, out_stream->codec->extradata_size);


            // copy input extradata to output
            memcpy(out_stream->codec->extradata, in_stream->codec->extradata, in_stream->codec->extradata_size);
            out_stream->codec->extradata_size = in_stream->codec->extradata_size;

            // FIXME: test copy input extradata to output 
            /*
            uint8_t *extradata = NULL;
            extradata = malloc(out_stream->codec->extradata_size + in_stream->codec->extradata_size + 32);
            if(extradata == NULL){
                LOG_ERROR("Failed to malloc for new extradata.\n");
                return -1;
            }
            memcpy(extradata, in_stream->codec->extradata, in_stream->codec->extradata_size);
            memcpy(extradata+in_stream->codec->extradata_size, out_stream->codec->extradata, out_stream->codec->extradata_size);

            free(out_stream->codec->extradata);
            out_stream->codec->extradata_size += in_stream->codec->extradata_size;
            */

        }else{
            LOG_DEBUG("slice mode no need encode, just copy it.\n");

            out_stream = avformat_new_stream(obj->output_ctx, in_stream->codec->codec);
            if (!out_stream) {
                LOG_ERROR("Failed allocating output stream\n");
                return -1;
            }

            ret = 0;
            ret = avcodec_copy_context(out_stream->codec, in_stream->codec);
            if (ret < 0) {
                LOG_ERROR("Failed to copy context from input to output stream codec context\n");
                return -1;
            }

            // set some params that should just copy from in_stream 
            out_stream->codec->codec_tag = 0;

            if (obj->output_ctx->oformat->flags & AVFMT_GLOBALHEADER){
                LOG_DEBUG("AVFMT_GLOBALHEADER is True.\n");
                out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
            }
        }




        if ((in_stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) && (obj->first_video_stream_index == 0)){
            obj->first_video_stream_index = i;
        }

 

    }


    // copy the metadata
    AVDictionaryEntry *t = NULL;
    while ((t = av_dict_get(obj->input_ctx->metadata, "", t, AV_DICT_IGNORE_SUFFIX))){
        LOG_DEBUG("metadata: %s=%s.\n", t->key, t->value);
        av_dict_set(&obj->output_ctx->metadata, t->key, t->value, AV_DICT_DONT_OVERWRITE);
    }

    // for test
    obj->output_ctx->flags &= AVFMT_FLAG_MP4A_LATM;

    av_dump_format(obj->output_ctx, 0, obj->params.output_url, 1);

    if (avio_open(&obj->output_ctx->pb, obj->params.output_url, AVIO_FLAG_WRITE) < 0){
        LOG_ERROR("open output file:%s error.\n", obj->params.output_url);
        return -1;
    }

    ret = avformat_write_header(obj->output_ctx, NULL);
    if( ret < 0){
        LOG_ERROR("output_ctx av_write_header error, ret=%d!\n", ret);
        return ret;
    }

    return 0;
}

/** init the slicer **/
static int Slicer_init(Slicer_t *obj, Slicer_Params_t * params)
{

    // init object
    memset(obj, 0, sizeof(Slicer_t));
    INIT_LIST_HEAD(&obj->packets_queue_head);


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

    /** open the output media **/
    if(Slicer_open_output_media(obj) < 0){
        LOG_ERROR("open the output media error.\n");
        return -1;
    }

    /** init the times, use AV_TIME_BASE=1000000 **/
    obj->slice_start_time = obj->params.start_time * 1000;
    obj->slice_end_time   = obj->params.end_time * 1000; 


    return 0;
}


static int Slicer_seek_start_time_key_frame(Slicer_t *obj)
{
    int ret ;

    // seek the start time, AV_TIME_BASE=1000000 
    ret = av_seek_frame(obj->input_ctx, -1, obj->slice_start_time, AVSEEK_FLAG_BACKWARD);
    if (ret < 0) {
        LOG_ERROR("could not seek position %lld\n", obj->slice_start_time);
        return -1;
    }
    LOG_DEBUG("Success to seek position %lld\n", obj->slice_start_time);

    return 0;
}

static int Slicer_rebuild_packet_timestamp(Slicer_t *obj, AVPacket *pkt)
{

    // input stream timebase
    AVRational input_timebase = obj->input_ctx->streams[pkt->stream_index]->time_base;

    // output stream timebase
    AVRational output_timebase = obj->output_ctx->streams[pkt->stream_index]->time_base;

    int64_t first_video_frame_pts = av_rescale_q(obj->first_video_frame_pts, AV_TIME_BASE_Q, input_timebase);
    int64_t first_video_frame_dts = av_rescale_q(obj->first_video_frame_dts, AV_TIME_BASE_Q, input_timebase);
    //int64_t first_video_frame_pts = obj->first_video_frame_pts_input_timebase;
    //int64_t first_video_frame_dts = obj->first_video_frame_dts_input_timebase;

    // skip the no need packets
    if( (pkt->pts - first_video_frame_pts < 0) || (pkt->dts - first_video_frame_dts < 0)) {
        LOG_DEBUG("stream_index:%d, pkt->pts(%lld) - first_video_frame_pts(%lld) < 0 || pkt->dts(%lld) - first_video_frame_dts(%lld) < 0\n",
            pkt->stream_index, pkt->pts, first_video_frame_pts, pkt->dts, first_video_frame_dts);
        return -1;
    }

    // rebuild the timestamp
    if(pkt->pts != AV_NOPTS_VALUE){
        pkt->pts = av_rescale_q_rnd(pkt->pts - first_video_frame_pts, input_timebase, output_timebase, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    }
    if(pkt->dts != AV_NOPTS_VALUE){
        pkt->dts = av_rescale_q_rnd(pkt->dts - first_video_frame_dts, input_timebase, output_timebase, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    }
    pkt->duration = 1; // for test
    if(pkt->duration > 0){
        pkt->duration = av_rescale_q(pkt->duration, input_timebase, output_timebase);
    }
    if(pkt->convergence_duration){
        pkt->convergence_duration = av_rescale_q(pkt->convergence_duration, input_timebase, output_timebase);
    }
    pkt->pos = -1;

    return 0;
}

static int Slicer_slice_copy_only(Slicer_t *obj)
{

    // seek to start time key frame
    if (Slicer_seek_start_time_key_frame(obj) < 0){
        LOG_ERROR("Slicer_seek_start_time_key_frame error.\n");
        return -1;
    }

    int ret;
    AVPacket pkt;
    int64_t first_video_frame_pts = 0;
    int64_t first_video_frame_dts = 0;

    int64_t slice_end_time = obj->slice_end_time;

    // get input stream timebase
    int input_timebase_num;
    int input_timebase_den;

    while(1){
        // read frame
        ret = av_read_frame(obj->input_ctx, &pkt);
        if (ret == AVERROR(EAGAIN)){
            LOG_INFO("av_read_frame ret=%d, EAGAIN means try again.\n", ret);
            continue;
        }

        if (ret == AVERROR_EOF){
            LOG_INFO("av_read_frame ret=AVERROR_EOF, break.\n");
            break;
        }

        if (ret < 0){
            LOG_INFO("av_read_frame ret=%d, break.\n", ret);
            break;
        }        

        // get the first video key frame to start
        if (first_video_frame_pts == 0){

            // if the pkt is video and is key frame
            if ( (obj->output_ctx->streams[pkt.stream_index]->codec->codec_type == AVMEDIA_TYPE_VIDEO) && (pkt.flags & AV_PKT_FLAG_KEY)){
                LOG_INFO("Get the first video key frame, dts: %lld ,pts: %lld\n", pkt.dts, pkt.pts);
                first_video_frame_pts = pkt.pts;
                first_video_frame_dts = pkt.dts;

                slice_end_time = av_rescale_q_rnd(slice_end_time, AV_TIME_BASE_Q, obj->output_ctx->streams[pkt.stream_index]->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
                LOG_INFO("first_video_frame_pts: %lld\n", first_video_frame_pts);
                LOG_INFO("first_video_frame_dts: %lld\n", first_video_frame_dts);
                LOG_INFO("slice_end_time: %lld\n", slice_end_time);

            }else{
                LOG_DEBUG("Not first video key frame, codec_type:%d, dkt:%lld, pts:%lld.\n", obj->output_ctx->streams[pkt.stream_index]->codec->codec_type, pkt.dts, pkt.pts);
            }
        }

        if ( first_video_frame_pts != 0 ) {

            // rebuild the timestamp
            pkt.pts = av_rescale_q_rnd(pkt.pts, obj->input_ctx->streams[pkt.stream_index]->time_base, obj->output_ctx->streams[pkt.stream_index]->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
            pkt.dts = av_rescale_q_rnd(pkt.dts, obj->input_ctx->streams[pkt.stream_index]->time_base, obj->output_ctx->streams[pkt.stream_index]->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
            pkt.duration = av_rescale_q(pkt.duration, obj->input_ctx->streams[pkt.stream_index]->time_base, obj->output_ctx->streams[pkt.stream_index]->time_base);
            pkt.pos = -1;

            // check the end time 
            if( (obj->output_ctx->streams[pkt.stream_index]->codec->codec_type == AVMEDIA_TYPE_VIDEO) && pkt.pts > slice_end_time){
                LOG_INFO("reach the slice_end_time: %lld, pts: %lld\n", slice_end_time, pkt.pts);
                break;
            }

            // write frame
            ret = av_interleaved_write_frame(obj->output_ctx, &pkt);
            if (ret < 0) {
                LOG_ERROR("av_interleaved_write_frame error\n");
                break;
            }


        }

        // free the frame
        av_free_packet(&pkt);
    }

    av_write_trailer(obj->output_ctx);

    return 0;
}


static int Slicer_slice_encode_only(Slicer_t *obj)
{

    // seek to start frame

    // read frame

    // decode video frame

    // encode video frame

    // write frame

    return 0;
}



/***
    Read a group of frames, and check it if or not the start group or end group.
    @group_type:    0 , means normal group
                    1 , means start group
                    2 , means end group

***/
static int Slicer_read_group(Slicer_t *obj, list_t * packets_queue, int * group_type)
{
    if (packets_queue == NULL){
        LOG_ERROR("packets_queue is NULL.\n");
        return -1;
    }

    if (group_type == NULL){
        LOG_ERROR("group_type is NULL.\n");
        return -1;
    }

    // init args
    INIT_LIST_HEAD(packets_queue);
    *group_type = 0;



    int i;
    int ret;
    Slicer_Packet_t * spkt = NULL;
    int video_key_frame_number = 0;   // the video key frame number of tmp queue
    int out_queue_has_video_key = 0;  // the video key frame number in out queue



    // get packets which in queue
    while(obj->packets_queue_head.next != &obj->packets_queue_head){
        spkt = obj->packets_queue_head.next;
        // delete from queue
        list_del(spkt);
        // add to new queue
        list_add_tail(spkt, packets_queue);

        if(spkt->packet.flags & AV_PKT_FLAG_KEY){
            video_key_frame_number += 1;
            out_queue_has_video_key += 1;
        }

        spkt = NULL;
    }

    int64_t slice_end_time = obj->slice_end_time;
    int media_end_of_file = 0;

    // read packets
    spkt = NULL;
    while(1){
        // 
        if (video_key_frame_number > 1){
            //LOG_DEBUG("video_key_frame_number(%d) > 1, break.\n", video_key_frame_number);
            break;
        }

        // malloc packet element for queue
        if (spkt == NULL){
            spkt = (Slicer_Packet_t *)malloc(sizeof(Slicer_Packet_t));
            if (spkt == NULL){
                LOG_ERROR("malloc Slicer_Packet_t error, return is NULL.\n");
                return -1;
            }
            memset(spkt, 0, sizeof(Slicer_Packet_t));
        }

        // read frame
        ret = av_read_frame(obj->input_ctx, &spkt->packet);
        if (ret == AVERROR(EAGAIN)){
            LOG_INFO("av_read_frame ret=%d, EAGAIN means try again.\n", ret);
            continue;
        }

        if (ret == AVERROR_EOF){
            LOG_INFO("av_read_frame ret=AVERROR_EOF, break.\n");
            free(spkt);
            media_end_of_file = 1;
            break;
        }

        if (ret < 0){
            LOG_INFO("av_read_frame ret=%d, break.\n", ret);
            free(spkt);
            media_end_of_file = 1;
            break;
        }   

        if( (spkt->packet.flags & AV_PKT_FLAG_KEY)  && (obj->first_video_stream_index == spkt->packet.stream_index) ){
            video_key_frame_number += 1;
        }

        // add to packets_queue_head
        list_add_tail(spkt, &obj->packets_queue_head);



        spkt = NULL;
    }

    // get packets which in tmp queue
    while(obj->packets_queue_head.next != &obj->packets_queue_head){
        spkt = obj->packets_queue_head.next;

        // if packet dts>=end_time, set group_type
        slice_end_time = av_rescale_q_rnd(obj->slice_end_time, AV_TIME_BASE_Q, obj->output_ctx->streams[spkt->packet.stream_index]->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
        if(spkt->packet.dts >= slice_end_time){
            LOG_INFO("find the end, spkt->packet.dts(%lld) >= slice_end_time(%lld).\n", spkt->packet.dts, slice_end_time);
            media_end_of_file = 1;
        }
    
        // until video key frame
        if( (spkt->packet.flags & AV_PKT_FLAG_KEY) && (obj->first_video_stream_index == spkt->packet.stream_index) && (out_queue_has_video_key > 0) ){
            //LOG_DEBUG(" break.\n");
            break;
        }

        // delete from queue
        list_del(spkt);
        // add to new queue 
        list_add_tail(spkt, packets_queue);

        // check the packet if or not the video key frame
        if ((spkt->packet.flags & AV_PKT_FLAG_KEY) && (obj->first_video_stream_index == spkt->packet.stream_index)){
            out_queue_has_video_key += 1;
        }

        spkt = NULL;
    }


    // set group type
    if (obj->packets_group_count == 0){
        *group_type = 1;
        obj->packets_group_count = 1;

    }else{
        obj->packets_group_count += 1;
        *group_type = 0;
    }

    if(media_end_of_file){
        obj->packets_group_count += 1;
        *group_type = 2;
    }
    return 0;
}



static int Slicer_video_transcode_group(Slicer_t *obj, list_t * group_head, int group_type)
{
    list_t tmp_video_head;
    list_t tmp_audio_head;
    list_t timestamp_head;

    Slicer_Packet_t * spkt = NULL;
    Slicer_timestamp_t * tsptr = NULL;

    AVFrame  frame;
    int got_dec_frame;
    int got_enc_frame;
    int ret;
    int packet_skip_flag;


    int64_t slice_start_time = av_rescale_q_rnd(obj->slice_start_time, AV_TIME_BASE_Q, obj->input_ctx->streams[obj->first_video_stream_index]->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
    int64_t slice_end_time   = av_rescale_q_rnd(obj->slice_end_time, AV_TIME_BASE_Q, obj->input_ctx->streams[obj->first_video_stream_index]->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);



    INIT_LIST_HEAD(&tmp_video_head);
    INIT_LIST_HEAD(&tmp_audio_head);
    INIT_LIST_HEAD(&timestamp_head);

    LOG_INFO("slice_start_time=%lld, slice_end_time=%lld.\n", slice_start_time, slice_end_time);

    // default skip the no need packet
    packet_skip_flag = 0;

    // while has packets
    while(group_head->next != group_head){
        spkt = group_head->next;
        //LOG_DEBUG("packet stream_index:%d, dts=%lld. \n", spkt->packet.stream_index, spkt->packet.dts);

        if(spkt->packet.stream_index == obj->first_video_stream_index){
            //LOG_DEBUG("to decode the packet. \n");
            LOG_DEBUG("packet stream_index:%d, pts=%lld,dts=%lld. \n", spkt->packet.stream_index,  spkt->packet.pts, spkt->packet.dts);
            got_dec_frame = 0;
            memset(&frame, 0, sizeof(frame));
            frame.extended_data = NULL;
            //get_frame_defaults(&frame);

            // push the packet timestamp to queue
            tsptr = malloc(sizeof(Slicer_timestamp_t));
            memset(tsptr, 0, sizeof(Slicer_timestamp_t));
            tsptr->pts = spkt->packet.pts;
            tsptr->dts = spkt->packet.dts;
            list_add_tail(tsptr, &timestamp_head);


            // decode the video 
            ret = avcodec_decode_video2(obj->input_ctx->streams[obj->first_video_stream_index]->codec, &frame, &got_dec_frame, &spkt->packet);
            if (ret < 0) {
                LOG_ERROR("avcodec_decode_video2 failed, ret=%d\n", ret);
                break;
            }
            // pop the packet from group_head, the spkt not free for contain encoded packet
            list_del(spkt);

            LOG_DEBUG("after decode ret=%d, got_dec_frame=%d. \n", ret, got_dec_frame);

            // encode
            if(got_dec_frame > 0){
                // get the packet pts
                if(timestamp_head.next != &timestamp_head){
                    tsptr = timestamp_head.next;
                    frame.pts = frame.pkt_pts = tsptr->pts;
                    frame.pkt_dts = tsptr->dts;
                    list_del(tsptr);
                    free(tsptr);
                    tsptr = NULL;
                }

                LOG_DEBUG("frame pts=%lld , pkt_pts=%lld, pkt_dts=%lld. \n", frame.pts, frame.pkt_pts, frame.pkt_dts);
                // check the packet should be skip
                if (group_type == SLICER_GROUP_TYPE_START){
                    // if the packet timestamp < start_time, skip it.
                    if (frame.pts < slice_start_time){
                        LOG_DEBUG("frame.pts(%lld) < slice_start_time(%lld), continue. \n", frame.pts, slice_start_time);
                        free(spkt);
                        continue;
                    }else{
                        LOG_DEBUG("frame.pts(%lld) >= slice_start_time(%lld), to encode. \n", frame.pts, slice_start_time);
                        packet_skip_flag = 1;
                    }

                }

                if (group_type == SLICER_GROUP_TYPE_END){
                    // if the packet timestamp > end_time , skip it.
                    if (frame.pts > slice_start_time){
                        LOG_DEBUG("frame.pts > slice_start_time, packet stream_index:%d, dts=%lld, at the end, break. \n", spkt->packet.stream_index, spkt->packet.dts);
                        free(spkt);
                        break;
                    }else{
                        packet_skip_flag = 1;
                    }
                }


                //LOG_DEBUG("got decoded frame to encode it. \n");
                spkt->packet.data = NULL;
                spkt->packet.size = 0;
                av_init_packet(&spkt->packet);

                

                got_enc_frame = 0;
                ret = avcodec_encode_video2(obj->output_ctx->streams[obj->first_video_stream_index]->codec, &spkt->packet, &frame, &got_enc_frame);
                if (ret < 0){
                    LOG_ERROR("avcodec_encode_video2 failed, ret=%d\n", ret);
                    break;
                }
                LOG_DEBUG("after encoded ret=%d, got_enc_frame=%d. \n", ret, got_enc_frame);

                if (got_enc_frame > 0){
                    spkt->packet.stream_index = obj->first_video_stream_index;
                    LOG_DEBUG("got encoded pkt, stream_index:%d, pts:%lld, dts:%lld, is_key:%d, size:%d \n", 
                                spkt->packet.stream_index, spkt->packet.pts, spkt->packet.dts, spkt->packet.flags&AV_PKT_FLAG_KEY, 
                                spkt->packet.size);

                    // push the packet to tmp_video_head
                    list_add_tail(spkt, &tmp_video_head);

                    if (obj->first_video_frame_pts == 0){
                        obj->first_video_frame_pts = av_rescale_q_rnd(spkt->packet.pts, obj->input_ctx->streams[obj->first_video_stream_index]->time_base, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
                        obj->first_video_frame_dts = av_rescale_q_rnd(spkt->packet.dts, obj->input_ctx->streams[obj->first_video_stream_index]->time_base, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
                        obj->first_video_frame_pts_input_timebase = spkt->packet.pts;
                        obj->first_video_frame_dts_input_timebase = spkt->packet.dts;
                        LOG_DEBUG("first_video_frame_pts:%lld, first_video_frame_dts:%lld.\n",  obj->first_video_frame_pts, obj->first_video_frame_dts);
                        LOG_DEBUG("first_video_frame_pts_input_timebase:%lld, first_video_frame_dts_input_timebase:%lld.\n",  obj->first_video_frame_pts_input_timebase, obj->first_video_frame_dts_input_timebase);
                        LOG_DEBUG("[OUT] encoder extradata_size:%d.\n",  obj->output_ctx->streams[obj->first_video_stream_index]->codec->extradata_size);
                        //print_hex(obj->output_ctx->streams[obj->first_video_stream_index]->codec->extradata, obj->output_ctx->streams[obj->first_video_stream_index]->codec->extradata_size);
                    }

                }else{
                    free(spkt);
                }

            }else{
                // the spkt no need, to free it
                LOG_DEBUG("not got encoded frame, to free the spkt. \n", ret, got_dec_frame);
                free(spkt);
            }

            // if get encoded frame, add it to tmp_head

        }else{
            
            if (0 && packet_skip_flag == 1){
                LOG_DEBUG("packet_skip_flag == 1, packet stream_index:%d, dts=%lld. \n", spkt->packet.stream_index, spkt->packet.dts);
                list_del(spkt);
                free(spkt);
            }else{
                // move the packets to tmp_audio_head
                list_del(spkt);
                list_add_tail(spkt, &tmp_audio_head);
            }
        }

    }

    // flush the decoder and encoder
    LOG_INFO("Flushing stream #%u decoder\n", obj->first_video_stream_index);
    while (1) {
        
        spkt = malloc(sizeof(Slicer_Packet_t));
        if ( spkt == NULL){
            LOG_ERROR("malloc Slicer_Packet_t failed, ret=%d\n");
            break;
        }

        memset(spkt, 0, sizeof(Slicer_Packet_t));

        spkt->packet.data = NULL;
        spkt->packet.size = 0;
        av_init_packet(&spkt->packet);


//
        got_dec_frame = 0;
        memset(&frame, 0, sizeof(frame));
        frame.extended_data = NULL;

        // decode the video 
        ret = avcodec_decode_video2(obj->input_ctx->streams[obj->first_video_stream_index]->codec, &frame, &got_dec_frame, &spkt->packet);
        if (ret < 0) {
            LOG_ERROR("avcodec_decode_video2 failed, ret=%d\n", ret);
            break;
        }
        // pop the packet from group_head, the spkt not free for contain encoded packet

        if (ret == 0){
            LOG_INFO("There is no data to be decoded, break.\n");
            free(spkt);
            break;
        }

        LOG_DEBUG("after decode ret=%d, got_dec_frame=%d. \n", ret, got_dec_frame);

        // encode
        if(got_dec_frame > 0){
            // FIXME: THE decoded frame should to encode !!
            // get the packet pts
            if(timestamp_head.next != &timestamp_head){
                tsptr = timestamp_head.next;
                frame.pts = frame.pkt_pts = tsptr->pts;
                frame.pkt_dts = tsptr->dts;
                list_del(tsptr);
                free(tsptr);
                tsptr = NULL;
            }
            //LOG_DEBUG("got decoded frame to encode it, pts=%lld, to encode. \n", frame.pts); 
            spkt->packet.size = 0;
            av_init_packet(&spkt->packet);
            spkt->packet.stream_index = obj->first_video_stream_index;

            got_enc_frame = 0;
            ret = avcodec_encode_video2(obj->output_ctx->streams[obj->first_video_stream_index]->codec, &spkt->packet, &frame, &got_enc_frame);
            if (ret < 0){
                LOG_ERROR("avcodec_encode_video2 failed, ret=%d\n", ret);
                break;
            }

            if (got_enc_frame > 0){
                LOG_DEBUG("got encoded frame to push it to queue. \n");
                // push the packet to tmp_video_head
                list_add_tail(spkt, &tmp_video_head);

                if (obj->first_video_frame_pts == 0){
                    obj->first_video_frame_pts = av_rescale_q_rnd(spkt->packet.pts, obj->input_ctx->streams[obj->first_video_stream_index]->time_base, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
                    obj->first_video_frame_dts = av_rescale_q_rnd(spkt->packet.dts, obj->input_ctx->streams[obj->first_video_stream_index]->time_base, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
                    obj->first_video_frame_pts_input_timebase = spkt->packet.pts;
                    obj->first_video_frame_dts_input_timebase = spkt->packet.dts;
                    LOG_DEBUG("first_video_frame_pts:%lld, first_video_frame_dts:%lld.\n",  obj->first_video_frame_pts, obj->first_video_frame_dts);
                    LOG_DEBUG("first_video_frame_pts_input_timebase:%lld, first_video_frame_dts_input_timebase:%lld.\n",  obj->first_video_frame_pts_input_timebase, obj->first_video_frame_dts_input_timebase);
                    LOG_DEBUG("[OUT] encoder extradata_size:%d.\n",  obj->output_ctx->streams[obj->first_video_stream_index]->codec->extradata_size);
                    //print_hex(obj->output_ctx->streams[obj->first_video_stream_index]->codec->extradata, obj->output_ctx->streams[obj->first_video_stream_index]->codec->extradata_size);
                }

            }else{
                free(spkt);
            }

        }else{
            // the spkt no need, to free it
            LOG_DEBUG("not got encoded frame, to free the spkt. \n", ret, got_dec_frame);
            free(spkt);
        }



    }

    //  flush the encoder
    LOG_INFO("Flushing stream #%u encoder\n", obj->first_video_stream_index);
    while(1){
        
        spkt = malloc(sizeof(Slicer_Packet_t));
        if ( spkt == NULL){
            LOG_ERROR("malloc Slicer_Packet_t failed, ret=%d\n");
            break;
        }

        memset(spkt, 0, sizeof(Slicer_Packet_t));

        spkt->packet.data = NULL;
        spkt->packet.size = 0;
        av_init_packet(&spkt->packet);

        got_enc_frame = 0;
        ret = avcodec_encode_video2(obj->output_ctx->streams[obj->first_video_stream_index]->codec, &spkt->packet, NULL, &got_enc_frame);
        if (ret < 0){
            LOG_ERROR("avcodec_encode_video2 failed, ret=%d\n", ret);
            break;
        }

        if (got_enc_frame > 0){
            //LOG_DEBUG("got encoded frame to push it to queue. \n");
            spkt->packet.stream_index = obj->first_video_stream_index;
            LOG_DEBUG("Flushing,got encoded pkt, stream_index:%d, pts:%lld, dts:%lld, is_key:%d, size:%d \n", 
                        spkt->packet.stream_index, spkt->packet.pts, spkt->packet.dts, spkt->packet.flags&AV_PKT_FLAG_KEY, 
                        spkt->packet.size);
            // push the packet to tmp_video_head
            list_add_tail(spkt, &tmp_video_head);
            if (obj->first_video_frame_pts == 0){
                obj->first_video_frame_pts = av_rescale_q_rnd(spkt->packet.pts, obj->input_ctx->streams[obj->first_video_stream_index]->time_base, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
                obj->first_video_frame_dts = av_rescale_q_rnd(spkt->packet.dts, obj->input_ctx->streams[obj->first_video_stream_index]->time_base, AV_TIME_BASE_Q, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
                obj->first_video_frame_pts_input_timebase = spkt->packet.pts;
                obj->first_video_frame_dts_input_timebase = spkt->packet.dts;
                LOG_DEBUG("first_video_frame_pts:%lld, first_video_frame_dts:%lld.\n",  obj->first_video_frame_pts, obj->first_video_frame_dts);
                LOG_DEBUG("first_video_frame_pts_input_timebase:%lld, first_video_frame_dts_input_timebase:%lld.\n",  obj->first_video_frame_pts_input_timebase, obj->first_video_frame_dts_input_timebase);
                LOG_DEBUG("[OUT] encoder extradata_size:%d.\n",  obj->output_ctx->streams[obj->first_video_stream_index]->codec->extradata_size);
                //print_hex(obj->output_ctx->streams[obj->first_video_stream_index]->codec->extradata, obj->output_ctx->streams[obj->first_video_stream_index]->codec->extradata_size);
            }
        }else{
            LOG_INFO("There is no data that has been encoded, break.\n");
            free(spkt);
            break;
        }
    }


    // move the packets to group_head
    LOG_DEBUG("to move the packets to group_head. \n");
    while((tmp_audio_head.next != &tmp_audio_head) || (tmp_video_head.next != &tmp_video_head)){
        spkt = tmp_video_head.next;
        if (spkt != &tmp_video_head){
            list_del(spkt);
            list_add_tail(spkt, group_head);
            LOG_DEBUG("packet stream_index:%d, key=%d, dts=%lld, pts=%lld, size=%d. \n", 
                spkt->packet.stream_index, spkt->packet.flags&AV_PKT_FLAG_KEY, spkt->packet.dts, spkt->packet.pts, spkt->packet.size);
        }
        spkt = tmp_audio_head.next;
        if (spkt != &tmp_audio_head){
            list_del(spkt);
            list_add_tail(spkt, group_head);
            //LOG_DEBUG("packet stream_index:%d, dts=%lld. \n", spkt->packet.stream_index, spkt->packet.dts);
        }
    }


    return 0;
}

static int Slicer_slice_encode_copy_encode(Slicer_t *obj)
{

    // seek to start time key frame
    if (Slicer_seek_start_time_key_frame(obj) < 0){
        LOG_ERROR("Slicer_seek_start_time_key_frame error.\n");
        return -1;
    }

    int ret;
    int group_type = 0;
    list_t packets_queue;
    Slicer_Packet_t * spkt = NULL;


    // for test, to get the video and audio packet number in one group
    int video_packets_number = 0;
    int audio_packets_number = 0;
    int test_cnt = 0;
    while(1){
        // read a gop, and to check if start or end gop. (start or end gop need transcode)
        ret = Slicer_read_group(obj, &packets_queue, &group_type);
        //LOG_DEBUG("Slicer_read_group ret:%d, group_type:%d \n", ret, group_type);

        if(packets_queue.next == &packets_queue){
            LOG_INFO("not get packet group, break.\n");
            break;
        }

        // for test, to get the video and audio packet number in one group
        int video_packets_number = 0;
        int audio_packets_number = 0;
        


        // transcode when start group
        if ( group_type == SLICER_GROUP_TYPE_START ){
            LOG_INFO("this is the start group \n");
            Slicer_video_transcode_group(obj, &packets_queue, 1);
        }

        if ( group_type == SLICER_GROUP_TYPE_END ){
            LOG_INFO("this is the end group \n");
            Slicer_video_transcode_group(obj, &packets_queue, 2);
        }

        // write frame
        while(packets_queue.next != &packets_queue){
            //LOG_DEBUG("Slicer_slice_encode_copy_encode test line\n");


            spkt = packets_queue.next;
            //if( (spkt->packet.flags & AV_PKT_FLAG_KEY) && (obj->first_video_stream_index == spkt->packet.stream_index))
            if(1 && (obj->first_video_stream_index == spkt->packet.stream_index)){
                LOG_DEBUG("before rebuild, get packet: stream_index:%d, pts:%lld, dts:%lld, is_key:%d, duration:%d,buf:%d \n", 
                            spkt->packet.stream_index, spkt->packet.pts, spkt->packet.dts, spkt->packet.flags&AV_PKT_FLAG_KEY, 
                            spkt->packet.duration, spkt->packet.buf);
            }
            /*else{
                av_free_packet(&spkt->packet);

                list_del(spkt);
                free(spkt); 
                continue;
            }*/



            // rebuild timestamp
            if(Slicer_rebuild_packet_timestamp(obj, &spkt->packet) >= 0){
                if(1 && (obj->first_video_stream_index == spkt->packet.stream_index))
                    LOG_DEBUG("after  rebuild, get packet: stream_index:%d, pts:%lld, dts:%lld, is_key:%d, duration:%d,buf:%d  \n", 
                                spkt->packet.stream_index, spkt->packet.pts, spkt->packet.dts, spkt->packet.flags&AV_PKT_FLAG_KEY,
                                spkt->packet.duration, spkt->packet.buf);
                // write
                ret = av_interleaved_write_frame(obj->output_ctx, &spkt->packet);
                //ret = av_write_frame(obj->output_ctx, &spkt->packet);
                if (ret < 0) {
                    LOG_ERROR("av_interleaved_write_frame error:%d\n", ret);
                    break;
                }
            }


            av_free_packet(&spkt->packet);
            list_del(spkt);
            free(spkt);
        }


        // if the last group , break
        if(group_type == 2){
            LOG_INFO("this is the last group, break.\n");
            break;
        }


        if (test_cnt == 0){
            test_cnt += 1;
        }/*else{
            break; // for test
        }*/
    }

    av_write_trailer(obj->output_ctx);

    return 0;
}


int Slicer(Slicer_Params_t * params)
{
    Slicer_t slicer;

    //LOG_INFO("av_log_get_level:%d.\n", av_log_get_level());
    //av_log_set_level(AV_LOG_DEBUG);

    memset(&slicer, 0, sizeof(Slicer_t));

    if(Slicer_init(&slicer, params) < 0){
        LOG_ERROR("Slicer_init error.\n");
        return -1;
    }

    //return 0;

    // do slice
    switch(slicer.params.slice_mode){
        case SLICER_MODE_ENCODE_ONLY:
            LOG_INFO("do slice encode only.\n");

            break;
        case SLICER_MODE_COPY_ONLY:
            LOG_INFO("do slice copy only.\n");

            if(Slicer_slice_copy_only(&slicer) < 0){
                LOG_ERROR("Slicer_slice_copy_only error.\n");
                return -1;
            }

            break;
        case SLICER_MODE_ENCODE_COPY_ENCODE:
            LOG_INFO("do slice encode copy encode.\n");

            if(Slicer_slice_encode_copy_encode(&slicer) < 0){
                LOG_ERROR("Slicer_slice_encode_copy_encode error.\n");
                return -1;
            }

            break;
        default:
            LOG_INFO("do slice use default mode: copy only.\n");
            if(Slicer_slice_copy_only(&slicer) < 0){
                LOG_ERROR("Slicer_slice_copy_only error.\n");
                return -1;
            }

    }

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

    strcpy(params.input_url, "/root/video/Meerkats.mp4");
    strcpy(params.output_url, "s3.mp4");
    params.start_time = 17158;
    params.end_time = 30000;
    //params.slice_mode = SLICER_MODE_COPY_ONLY;
    params.slice_mode = SLICER_MODE_ENCODE_COPY_ENCODE;
    
    ret = Slicer(&params);
    LOG_DEBUG("slicer return: %d\n", ret);

    return 0;
}


