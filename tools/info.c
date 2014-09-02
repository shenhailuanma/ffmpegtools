
#include <stdio.h>

#include "libavformat/avformat.h"

static void print_help(void)
{
    printf("Usage: ./xspeed  -i <input_file> -s <speed_number> -o <output_file>\n");
    printf("\tinput_file: the input file.\n");
    printf("\tspeed_number: the number of speed change.\n");
    printf("\toutput_file: the output file.\n");
    printf("For example:\n");
    printf("\t./xspeed -i /data/video/source.mp4 -s 2 -o /tmp/out.mp4\n");
}


int get_info(char *src, char *param)
{


    int ret;
    int val;
    
    struct xspeed_params params;

    AVFormatContext *ctx = NULL;
    AVFormatContext *output_context = NULL;



    memset(&params, 0, sizeof(params));
    if(speed <= 0)
        speed = 1;

    params.input = src;
    params.output = dest;
    params.speed = speed;
    

    printf("input:%s\n", params.input);
    printf("output:%s\n", params.output);
    printf("speed:%f\n", params.speed);
    printf("format:%s\n", params.format);

    //params.speed = 2;

    av_register_all();

    // open the media file
    ret = avformat_open_input(&ctx, params.input, NULL, NULL);
    if (ret < 0) {
        ERR("error open stream: '%s', error code: %d \n", params.input, ret);
        return -1;
    }

    ret = avformat_find_stream_info(ctx, NULL);
    if (ret < 0) {
        ERR("could not find stream info.\n");
        return -1;
    }

    av_dump_format(ctx, 0, ctx->filename, 0);
    printf("input format:%s\n", ctx->iformat->name);
    strcpy(params.format, ctx->iformat->name);
    printf("output format:%s\n", params.format);

    int videoStreamIndex=-1;
    int audioStreamIndex=-1;
    double time_base = 0;
    int audio_time_base = 0;
    int video_time_base = 0;
    int i;
    AVCodecContext *video_decoder_ctx = NULL;

    for(i = 0; i < ctx->nb_streams; i++){
        AVStream* stream = ctx->streams[i];
        if(stream->codec->codec_type == AVMEDIA_TYPE_VIDEO){
            printf("find a video stream[%d]: %dkbps\n", i, stream->codec->bit_rate / 1000);
            videoStreamIndex = i;
            video_decoder_ctx = stream->codec;
            video_time_base = stream->time_base.den / stream->time_base.num;
            printf("video_time_base=%d\n", video_time_base);
        }
        if(stream->codec->codec_type == AVMEDIA_TYPE_AUDIO){
            printf("find a audio stream[%d]: %dkbps\n", i, stream->codec->bit_rate / 1000);
            audioStreamIndex = i;
           
            audio_time_base = stream->time_base.den / stream->time_base.num;
            printf("audio_time_base=%d\n", audio_time_base);
        }
    }


    // open the output 
    AVOutputFormat *output_format = NULL;
    if(strncmp(params.format, "ts", sizeof("ts")) == 0){
        output_format = av_guess_format("mpegts", NULL, NULL);
    } else if(strncmp(params.format, "flv", sizeof("flv")) == 0){
        output_format = av_guess_format("flv", NULL, NULL);
    } else {
        output_format = av_guess_format("mp4", NULL, NULL);
    }
    if (!output_format) {
        ERR("Could not find %s format", params.format);
        return -1;
    }

    output_context = avformat_alloc_context();
    if (!output_context) {
        ERR("Could not allocated output context");
        return -1;
    }
    output_context->oformat = output_format;


    AVStream *video_stream;
    AVCodecContext *video_enc_ctx;
    int video_extra_size;

    //AVMetadataTag *t = NULL;
    //while ((t = av_metadata_get(ctx->metadata, "", t, AV_METADATA_IGNORE_SUFFIX)))
    //    av_metadata_set2(&output_context->metadata, t->key, t->value, AV_METADATA_DONT_OVERWRITE);

    if(video_decoder_ctx != NULL){
        video_stream = av_new_stream(output_context, output_context->nb_streams);
        video_stream->time_base = ctx->streams[videoStreamIndex]->time_base;
        avcodec_get_context_defaults3(video_stream->codec, NULL);
        video_enc_ctx = video_stream->codec;

        video_enc_ctx->codec_id = video_decoder_ctx->codec_id;
        video_enc_ctx->codec_type = video_decoder_ctx->codec_type;
        /*
        (!video_enc_ctx->codec_tag){
                if( !output_context->oformat->codec_tag
                   || av_codec_get_id (output_context->oformat->codec_tag, video_decoder_ctx->codec_tag) == video_enc_ctx->codec_id
                   || av_codec_get_tag(output_context->oformat->codec_tag, video_decoder_ctx->codec_id) <= 0)
                    video_enc_ctx->codec_tag = video_decoder_ctx->codec_tag;
        }
        */
        video_enc_ctx->bit_rate = video_decoder_ctx->bit_rate;
        video_enc_ctx->bit_rate_tolerance = video_decoder_ctx->bit_rate_tolerance;

        video_enc_ctx->rc_buffer_size = video_decoder_ctx->rc_buffer_size;
        video_enc_ctx->pix_fmt = video_decoder_ctx->pix_fmt;
        video_enc_ctx->time_base = ctx->streams[videoStreamIndex]->time_base;  

        video_enc_ctx->width = video_decoder_ctx->width;
       // video_enc_ctx->width = 640;
        video_enc_ctx->height = video_decoder_ctx->height;
       // video_enc_ctx->height = 480;
        video_enc_ctx->has_b_frames = video_decoder_ctx->has_b_frames;
	printf("width:%d, height:%d\n", video_enc_ctx->width, video_enc_ctx->height);
        
        if(output_context->oformat->flags & AVFMT_GLOBALHEADER)
            video_enc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

        video_extra_size = video_decoder_ctx->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE;
        video_enc_ctx->extradata = av_mallocz(video_extra_size);
        memcpy(video_enc_ctx->extradata, video_decoder_ctx->extradata, video_decoder_ctx->extradata_size);
        video_enc_ctx->extradata_size = video_decoder_ctx->extradata_size;
    }

    //if (av_set_parameters(output_context, NULL) < 0) {
    //    ERR("Invalid output format parameters\n");
    //    return -1;
    //}

    
    av_dump_format(output_context, 0, params.output, 1);

    if (avio_open(&output_context->pb, params.output, AVIO_FLAG_WRITE) < 0) {
        ERR("Could not open '%s'", params.output);
        return -1;
    }

    if (avformat_write_header(output_context,NULL)) {
        ERR("Could not write header to the output file.\n");
        return -1;
    }

    AVPacket packet;
    int frameIndex = 0;
    int frame_error = 0;

    // do 
    while(1){
 
        ret = av_read_frame(ctx, &packet);
        if(ret == AVERROR(EAGAIN)){
            // EAGAIN means try again

            continue;
        }
        if(ret < 0){
            break;
        }

        if(packet.stream_index == videoStreamIndex){
            frameIndex++;
            frame_error = 0;

            packet.pts = packet.pts * params.speed;
            packet.dts = packet.dts * params.speed;
            
            ret = av_interleaved_write_frame(output_context, &packet);
            if (ret < 0) {
                ERR("Could not write frame of stream: %d\n", ret);
                return -1;
            }
        }
        if(packet.stream_index == audioStreamIndex){
            frameIndex++;
        }

        av_free_packet(&packet);
    }

    // write the end
    av_write_trailer(output_context);


    // close ctx
    avformat_close_input(&output_context);
    avformat_close_input(&ctx);

    
    return 0;
}

