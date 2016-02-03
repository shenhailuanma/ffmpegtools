

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>


#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"


#include "libavutil/timestamp.h"



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

#define SLICER_TIME_BASE 


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

} Slicer_t;

typedef struct Slicer_Packet_t{
    struct list_head head;
    AVPacket packet;
    int number;
}Slicer_Packet_t;


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

    //obj->input_ctx->flags |= AVFMT_FLAG_GENPTS;
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
        out_stream->codec->codec_tag = 0;
        if (obj->output_ctx->oformat->flags & AVFMT_GLOBALHEADER){
            LOG_DEBUG("AVFMT_GLOBALHEADER is True.\n");
            out_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
        }

        if ((in_stream->codec->codec_type == AVMEDIA_TYPE_VIDEO) && (obj->first_video_stream_index == 0)){
            obj->first_video_stream_index = i;
        }

        LOG_DEBUG("codec id:%d\n", out_stream->codec->codec_id);
        LOG_DEBUG("codec name:%s\n", out_stream->codec->codec_name);
        LOG_DEBUG("codec codec:%d\n", out_stream->codec->codec);
        LOG_DEBUG("codec codec_type:%d\n", out_stream->codec->codec_type);
        LOG_DEBUG("codec codec_tag:%d\n", out_stream->codec->codec_tag);
        LOG_DEBUG("codec stream_codec_tag:%d\n", out_stream->codec->stream_codec_tag);
        LOG_DEBUG("codec av_class:%s\n", out_stream->codec->av_class->class_name);
    }


    // copy the metadata
    AVDictionaryEntry *t = NULL;
    while ((t = av_dict_get(obj->input_ctx->metadata, "", t, AV_DICT_IGNORE_SUFFIX))){
        LOG_DEBUG("metadata: %s=%s.\n", t->key, t->value);
        av_dict_set(&obj->output_ctx->metadata, t->key, t->value, AV_DICT_DONT_OVERWRITE);
    }


    // if slice mode need encode, should open the decoder and encoder
    if(obj->params.slice_mode == SLICER_MODE_ENCODE_ONLY || obj->params.slice_mode == SLICER_MODE_ENCODE_COPY_ENCODE){
        LOG_DEBUG("slice mode need encode, to open the encoder.\n");
        // open the encoder
    }else{
        LOG_DEBUG("slice mode no need encode, just copy it.\n");
    }


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

static int Slicer_slice_copy_only(Slicer_t *obj)
{

    // seek to start time key frame
    if (Slicer_seek_start_time_key_frame(obj) < 0){
        LOG_ERROR("Slicer_seek_start_time_key_frame error.\n");
        return -1;
    }

    int ret;
    AVPacket pkt;
    int64_t first_video_key_frame_pts = 0;
    int64_t first_video_key_frame_dts = 0;

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
        if (first_video_key_frame_pts == 0){

            // if the pkt is video and is key frame
            if ( (obj->output_ctx->streams[pkt.stream_index]->codec->codec_type == AVMEDIA_TYPE_VIDEO) && (pkt.flags & AV_PKT_FLAG_KEY)){
                LOG_INFO("Get the first video key frame, dts: %lld ,pts: %lld\n", pkt.dts, pkt.pts);
                first_video_key_frame_pts = pkt.pts;
                first_video_key_frame_dts = pkt.dts;

                slice_end_time = av_rescale_q_rnd(slice_end_time, AV_TIME_BASE_Q, obj->output_ctx->streams[pkt.stream_index]->time_base, AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX);
                LOG_INFO("first_video_key_frame_pts: %lld\n", first_video_key_frame_pts);
                LOG_INFO("first_video_key_frame_dts: %lld\n", first_video_key_frame_dts);
                LOG_INFO("slice_end_time: %lld\n", slice_end_time);

            }else{
                LOG_DEBUG("Not first video key frame, codec_type:%d, dkt:%lld, pts:%lld.\n", obj->output_ctx->streams[pkt.stream_index]->codec->codec_type, pkt.dts, pkt.pts);
            }
        }

        if ( first_video_key_frame_pts != 0 ) {

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


    int media_end_of_file = 0;

    // read packets
    spkt = NULL;
    while(1){
        // 
        if (video_key_frame_number > 1){
            LOG_DEBUG("video_key_frame_number(%d) > 1, break.\n", video_key_frame_number);
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
        // until video key frame
        if( (spkt->packet.flags & AV_PKT_FLAG_KEY) && (obj->first_video_stream_index == spkt->packet.stream_index) && (out_queue_has_video_key > 0) ){
            LOG_DEBUG(" break.\n");
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

    }else if(media_end_of_file){
        obj->packets_group_count += 1;
        *group_type = 2;
    }else{
        obj->packets_group_count += 1;
        *group_type = 0;
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
    while(1){
        // read a gop, and to check if start or end gop. (start or end gop need transcode)
        ret = Slicer_read_group(obj, &packets_queue, &group_type);
        LOG_DEBUG("Slicer_read_group ret:%d, group_type:%d \n", ret, group_type);

        if(packets_queue.next == &packets_queue){
            LOG_INFO("not get packet group, break.\n");
            break;
        }

        // for test, to get the video and audio packet number in one group
        int video_packets_number = 0;
        int audio_packets_number = 0;
        
        while(packets_queue.next != &packets_queue){
            //LOG_DEBUG("Slicer_slice_encode_copy_encode test line\n");

            spkt = packets_queue.next;
            if( (spkt->packet.flags & AV_PKT_FLAG_KEY) && (obj->first_video_stream_index == spkt->packet.stream_index))
                LOG_DEBUG("get packet: stream_index:%d, pts:%lld \n", spkt->packet.stream_index, spkt->packet.pts);

            list_del(spkt);

            free(spkt);

        }

        // transcode start or end gop

        // write frame


        // if the last group , break
        if(group_type == 2){
            LOG_INFO("this is the last group, break.\n");
            break;
        }
    }
    return 0;
}


int Slicer(Slicer_Params_t * params)
{
    Slicer_t slicer;

    memset(&slicer, 0, sizeof(Slicer_t));

    if(Slicer_init(&slicer, params) < 0){
        LOG_ERROR("Slicer_init error.\n");
        return -1;
    }


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

    strcpy(params.input_url, "/root/Meerkats.mp4");
    strcpy(params.output_url, "out.mp4");
    params.start_time = 10000;
    params.end_time = 30000;
    //params.slice_mode = SLICER_MODE_COPY_ONLY;
    params.slice_mode = SLICER_MODE_ENCODE_COPY_ENCODE;
    
    ret = Slicer(&params);
    LOG_DEBUG("slicer return: %d\n", ret);

    return 0;
}


