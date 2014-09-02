/*
 * this is used by videoedit
 *
 */


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "libavformat/avformat.h"
#include "libavformat/movenc.h"
#include "libavcodec/opt.h"
#include "libavcodec/avcodec.h"


#define TVIE_ENC_BUFFER_SIZE (1024*1024)
#define MAX_REORDER_DELAY 16

#define tvie_assert(expr) \
do { \
    if (unlikely(!(expr))) { \
        tvie_log(LOG_CRIT, "ASSERTION FAILURE: %s line %d, %s\n", \
				 __FILE__, __LINE__, #expr);					  \
        exit(-1); \
    } \
}while(0)

#define __tvie_debug(fmt, args ...) \
	tvie_log(LOG_DEBUG, "[DEBUG] " fmt "\n", ## args)

#define tvie_info(fmt, args ...) \
	tvie_log(LOG_INFO, fmt"\n", ## args)

#define __tvie_debug_nop(args ...) do {} while(0)

//#ifdef TVIE_DEBUG
#define tvie_debug __tvie_debug
#define xtvie_debug __tvie_debug_nop
#define xtvie_info __tvie_debug_nop


#ifndef TVIE_COLOR_RED
#define TVIE_COLOR_RED "\033[0;31;40m"
#endif
#ifndef TVIE_COLOR_GREEN
#define TVIE_COLOR_GREEN "\033[0;32;40m"
#endif
#ifndef TVIE_COLOR_YELLOW
#define TVIE_COLOR_YELLOW "\033[0;33;40m"
#endif
#ifndef TVIE_COLOR_NONE
#define TVIE_COLOR_NONE "\033[0m"
#endif


#define tvie_error(fmt, args ...) \
	tvie_log(LOG_ERR, "%sERROR%s: source: %s, line: %d, function: %s: "\
			 fmt "\n", TVIE_COLOR_RED, \
			 TVIE_COLOR_NONE, __FILE__, __LINE__, __FUNCTION__, ## args)

#define tvie_warning(fmt, args ...) \
	tvie_log(LOG_ERR, "%sWARNING%s: source: %s, line: %d: " \
			 fmt "\n", TVIE_COLOR_YELLOW, TVIE_COLOR_NONE, \
			 __FILE__, __LINE__, ## args)



int64_t xts = 0LL;
int64_t dts = 0LL;
int64_t global_video_pkt_pts = AV_NOPTS_VALUE;
int64_t first_picture_pts = AV_NOPTS_VALUE;
int64_t first_picture_xts = AV_NOPTS_VALUE;
int64_t picture_init_delay = AV_NOPTS_VALUE;
int64_t pts_buffer[MAX_REORDER_DELAY+1];
uint8_t *enc_buffer = NULL;
extern int ff_avc_parse_nal_units_buf(const uint8_t *buf_in, uint8_t **buf, int *size);
extern int ff_isom_write_avcc(ByteIOContext *pb, const uint8_t *data, int len);

int video_index = -1;
int audio_index = -1;

AVFormatContext *input_context = NULL;
AVFormatContext *output_context = NULL;

AVCodecContext *video_decoder_ctx = NULL;
AVCodecContext *audio_decoder_ctx = NULL;

AVStream *video_stream, *audio_stream;
AVCodecContext *video_enc_ctx, *audio_enc_ctx;
AVCodec *video_encoder, *audio_encoder;

#define MODE_COPY_ONLY 			1
#define MODE_ENCODE_ONLY		2
#define MODE_COPY_ENCODE		3
#define MODE_ENCODE_COPY		4
#define MODE_ENCODE_COPY_ENCODE	5

int our_get_buffer(struct AVCodecContext *c, AVFrame *pic)
{
	int ret = avcodec_default_get_buffer(c, pic);
	int64_t *pts = av_malloc(sizeof(int64_t));
	*pts = global_video_pkt_pts;
	pic->opaque = pts;

	return ret;
}

void our_release_buffer(struct AVCodecContext *c, AVFrame *pic)
{
	if (pic)
		av_freep(&pic->opaque);
	avcodec_default_release_buffer(c, pic);
}

int encode_video_frame(uint8_t* enc_buffer, AVFrame* picture)
{
	tvie_assert(video_enc_ctx != NULL);
		
	tvie_assert(enc_buffer != NULL);


	int enc_buf_size = avcodec_encode_video(video_enc_ctx, (uint8_t*) enc_buffer, TVIE_ENC_BUFFER_SIZE, picture);
	if (enc_buf_size < 0) {
		tvie_error("encode for picture failed, enc_buf_size: %d", enc_buf_size);
		return -1;
	}
	if (enc_buf_size == 0) {
		tvie_warning("encode for picture returned 0");
		return 0;
	}

	dts = 0LL;
	xts = video_enc_ctx->coded_frame->pts;
	if (first_picture_xts == AV_NOPTS_VALUE) {
		first_picture_xts = xts;
	}

	tvie_info("xts: %"PRId64"", xts);
	if (xts != AV_NOPTS_VALUE) {
		int i;
		int delay = FFMAX(video_enc_ctx->has_b_frames, !!video_enc_ctx->max_b_frames);
		int pkt_duration = (1000 / 25);        // duration per frame.

		tvie_assert(delay < MAX_REORDER_DELAY);

		pts_buffer[0] = xts;
		for (i = 1; i < delay + 1 && pts_buffer[i] == AV_NOPTS_VALUE; i++) {
			pts_buffer[i] = (i - delay - 1) * pkt_duration + first_picture_xts;
		}

		for (i = 0; i < delay && pts_buffer[i] > pts_buffer[i + 1]; i++) {
			FFSWAP(int64_t, pts_buffer[i], pts_buffer[i + 1]);
		}

		smp_rmb();      //avoid compiler over-optimizing

		//pop up the smallest pts as dts
		dts = pts_buffer[0];
	}

	return enc_buf_size;
}

int package_video_packet(int enc_buf_size, uint8_t* enc_buffer, AVPacket* avpkt)
{
	uint8_t *enc_buffer_out = av_malloc(enc_buf_size);
	if (enc_buffer_out == NULL) {
		tvie_error("malloc for video enc_buffer_out failed");
		exit(-1);
	}
	memcpy(enc_buffer_out, enc_buffer, enc_buf_size);

	//change startcode to nal length(mp4 format)
	int size = enc_buf_size;
	uint8_t *data = NULL;
	if (ff_avc_parse_nal_units_buf(enc_buffer_out, &data, &size) < 0) {
		tvie_error("nal convert failed");
		return -1;
	}

	av_free(enc_buffer_out);

	avpkt->stream_index = 0;
	avpkt->pts = xts;
	avpkt->dts = dts;
	avpkt->data = data;
	avpkt->size = size;
	avpkt->destruct = av_destruct_packet;
	if (video_enc_ctx->coded_frame) {
		if (video_enc_ctx->coded_frame->key_frame) {
			avpkt->flags |= AV_PKT_FLAG_KEY;
			tvie_info("encoded key frame.");
		}
	}			

	return 0;
}

int output_video_packet(AVPacket* avpkt, AVFormatContext *output_context)
{
    float pts_scale = (float)input_context->streams[video_index]->time_base.den / input_context->streams[video_index]->time_base.num / 25;
    avpkt->pts = (int)(avpkt->pts / pts_scale);
    avpkt->dts = (int)(avpkt->dts / pts_scale);
	tvie_info("out video pkt: pts: %"PRId64", dts:%"PRId64", size:%d", avpkt->pts, avpkt->dts, avpkt->size);
	int ret = av_interleaved_write_frame(output_context, avpkt);
	if (ret < 0) {
		tvie_error("tvie_transcoding_slicer: Could not write frame of stream: %d", ret);
	}
	else if (ret > 0) {
		tvie_error("tvie_transcoding_slicer: End of stream requested\n");
	}
	av_free_packet(avpkt);
	return ret;
}

int decode_video_packet(AVPacket* packet,  AVFrame* picture)
{
	tvie_info("decode video pkt.");
	avcodec_get_frame_defaults(picture);
	global_video_pkt_pts = packet->pts;
	int got_picture;
	int ret = avcodec_decode_video2(video_decoder_ctx, picture, &got_picture, packet);
	if (ret < 0) {
		tvie_error("video frame decode error for pkt: %"PRId64"", packet->pts);
		exit(-1);
	}

	int64_t pts = 0;
	if (picture->opaque && *(int64_t*) picture->opaque != AV_NOPTS_VALUE) {
		pts = *(int64_t *) picture->opaque;
	}

	if(!got_picture){
		tvie_info("no picture gotten.");
		return -1;
	}

	picture->pts = pts;
	picture->pict_type = 0;
	tvie_info("got video pic, type:%d, pts:%"PRId64"", picture->pict_type, picture->pts);

	if (first_picture_pts == AV_NOPTS_VALUE) {
		first_picture_pts = picture->pts;
	}

	return 0;
}

int output_data_copy(int64_t start_pts, int64_t end_pts)
{
	AVPacket packet;
	while(1){
		int read_done = av_read_frame(input_context, &packet);
		if (read_done < 0) {
			break;
		}

		if (packet.stream_index == video_index) {
			if (packet.flags & PKT_FLAG_KEY) {
				tvie_info("key frame!");
			}
			tvie_info("video pkt: pts: %"PRId64", dts:%"PRId64", size:%d", packet.pts, packet.dts, packet.size);
			output_video_packet(&packet, output_context);

		} // end of if (packet.stream_index == video_index)
		else if (packet.stream_index == audio_index){
			packet.stream_index = 1;
			tvie_info("audio pkt: pts: %"PRId64", dts:%"PRId64", size:%d", packet.pts, packet.dts, packet.size);
			int ret = av_interleaved_write_frame(output_context, &packet);
			if (ret < 0) {
				tvie_info("tvie_transcoding_slicer: Could not write frame of stream: %d", ret);
			}
			else if (ret > 0) {
				tvie_info("tvie_transcoding_slicer: End of stream requested");
				av_free_packet(&packet);
				break;
			}
		} // end of else (packet.stream_index == audio_index)

		av_free_packet(&packet);

	}

	return 0;
}

int output_packet_encode(AVPacket* packet)
{
	AVFrame picture;

	int ret = decode_video_packet(packet, &picture);
	if(ret == -1){
		tvie_info("no picture gotten.");
		return -1;
	}

	int enc_buf_size = encode_video_frame((uint8_t *)enc_buffer, &picture);
	if (enc_buf_size < 0) {
		tvie_error("encode for picture failed, enc_buf_size: %d", enc_buf_size);
		exit(-1);
	}
	if (enc_buf_size == 0) {
		tvie_warning("encode for picture returned 0");
		return -2;
	}

	av_free_packet(packet);
	package_video_packet(enc_buf_size, enc_buffer, packet);
	return 0;
}

int flush_decoder_data()
{
	int flush_done = 0;
	while(!flush_done){
		// dump buffered decoded picture.
		AVPacket avpkt;
		av_init_packet(&avpkt);
		avpkt.data = NULL;
		avpkt.size = 0;

		AVFrame picture;
		int ret = decode_video_packet(&avpkt, &picture);
		if(ret == -1){
			tvie_info("no picture gotten.");
			return -1;
		}

		int enc_buf_size = encode_video_frame((uint8_t *)enc_buffer, NULL);
		if (enc_buf_size < 0) {
			tvie_error("encode for picture failed, enc_buf_size: %d", enc_buf_size);
			exit(-1);
		}
		if (enc_buf_size == 0) {
			tvie_warning("flush_decoder_data encode for picture returned 0");
			flush_done = 1;
			break;
		}

		av_free_packet(&avpkt);
		package_video_packet(enc_buf_size, enc_buffer, &avpkt);
		output_video_packet(&avpkt, output_context);
	}

	return 0;
}

int flush_encoder_data()
{
	int flush_done = 0;
	while(!flush_done){
		AVPacket avpkt;
		av_init_packet(&avpkt);
		avpkt.data = NULL;
		avpkt.size = 0;
		int enc_buf_size = encode_video_frame((uint8_t*)enc_buffer, NULL);
		if (enc_buf_size < 0) {
			tvie_error("encode for picture failed, enc_buf_size: %d", enc_buf_size);
			exit(-1);
		}
		if (enc_buf_size == 0) {
			tvie_warning("flush_encoder_data encode for picture returned 0");
			flush_done = 1;
			break;
		}

		package_video_packet(enc_buf_size, enc_buffer, &avpkt);
		output_video_packet(&avpkt, output_context);
	}

	return 0;
}

void setup_video_codec()
{
	avcodec_get_context_defaults2(video_stream->codec, CODEC_TYPE_VIDEO);
	video_enc_ctx = video_stream->codec;
        video_enc_ctx->workaround_bugs = video_enc_ctx->workaround_bugs | FF_BUG_X264_I_SPS_ID;
	video_enc_ctx->codec_id = video_decoder_ctx->codec_id;
	tvie_info("video_decoder_ctx->codec_id: %d", video_decoder_ctx->codec_id);
	video_enc_ctx->codec_type = video_decoder_ctx->codec_type;
	if(!video_enc_ctx->codec_tag){
		if( !output_context->oformat->codec_tag
				|| av_codec_get_id (output_context->oformat->codec_tag, video_decoder_ctx->codec_tag) == video_enc_ctx->codec_id 
				|| av_codec_get_tag(output_context->oformat->codec_tag, video_decoder_ctx->codec_id) <= 0)
			video_enc_ctx->codec_tag = video_decoder_ctx->codec_tag;
	}

	video_encoder = avcodec_find_encoder(video_enc_ctx->codec_id);

	video_enc_ctx->bit_rate = video_decoder_ctx->bit_rate;
	video_enc_ctx->bit_rate_tolerance = video_decoder_ctx->bit_rate * 0.5;
	video_enc_ctx->rc_buffer_size = video_enc_ctx->bit_rate * 2;
	video_enc_ctx->rc_initial_buffer_occupancy = video_enc_ctx->rc_buffer_size * 0.9;

	video_enc_ctx->rc_max_rate = video_enc_ctx->bit_rate * 1.2;
	video_enc_ctx->rc_min_rate = video_enc_ctx->bit_rate * 0.9;
	video_enc_ctx->pix_fmt = video_decoder_ctx->pix_fmt;
	video_enc_ctx->time_base.num = 1;	//video_decoder_ctx->time_base.num;
	video_enc_ctx->time_base.den = 25;	//video_decoder_ctx->time_base.den;
	tvie_info("%d, %d", video_enc_ctx->time_base.num, video_enc_ctx->time_base.den);

	//video_enc_ctx->time_base = input_context->streams[video_index]->time_base;	

	video_enc_ctx->width = video_decoder_ctx->width;
	video_enc_ctx->height = video_decoder_ctx->height;

	video_enc_ctx->refs = 2;
	video_enc_ctx->coder_type = FF_CODER_TYPE_AC;    //CABAC
	video_enc_ctx->mb_decision = video_decoder_ctx->mb_decision;
	video_enc_ctx->b_frame_strategy = 0;
	video_enc_ctx->max_b_frames = 0;
	video_enc_ctx->me_range = 16;
	video_enc_ctx->gop_size = 250;
	video_enc_ctx->keyint_min = 25;

	video_enc_ctx->qmin = 10;
	video_enc_ctx->qmax = 51;
	video_enc_ctx->qcompress = 0.4;
	video_enc_ctx->directpred = 3;	
	video_enc_ctx->max_qdiff = 4;
	video_enc_ctx->thread_count = 6;
	video_enc_ctx->me_method = ME_HEX;
	video_enc_ctx->me_subpel_quality = 6;	
	video_enc_ctx->flags2 = CODEC_FLAG2_FASTPSKIP | CODEC_FLAG2_PSY;     // | CODEC_FLAG2_INTRA_REFRESH;

	video_enc_ctx->psy_rd = 1;
	video_enc_ctx->psy_trellis = 1;
	video_enc_ctx->aq_mode = 1;
	video_enc_ctx->aq_strength = 1.0;
	video_enc_ctx->rc_lookahead = 25 * 1.6;
	video_enc_ctx->me_cmp = FF_CMP_CHROMA;
	video_enc_ctx->weighted_p_pred = 2;

	if (video_enc_ctx->rc_lookahead > 0) {
		video_enc_ctx->flags2 |= CODEC_FLAG2_MBTREE;
	}	
	video_enc_ctx->partitions = X264_PART_I4X4 | X264_PART_I8X8
		| X264_PART_P4X4 | X264_PART_P8X8 | X264_PART_B8X8;
	video_enc_ctx->flags2 |= CODEC_FLAG2_MIXED_REFS;

	video_enc_ctx->chromaoffset = 0;
	video_enc_ctx->flags |= CODEC_FLAG_LOOP_FILTER;
	video_enc_ctx->deblockalpha = -2;
	video_enc_ctx->deblockbeta = -2;
	//if(output_context->oformat->flags & AVFMT_GLOBALHEADER)
	//	video_enc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	av_set_int(video_enc_ctx, "trellis", 1);

}
int main(int argc, char **argv)
{
	tvie_log_open(argv[0], 0, 0, TVIE_LOG_TARGET_STDIO);
	if (argc != 5) {
		fprintf(stderr, "Usage: %s <input filename> <output filename> <start time> <end time>\n", argv[0]);
		exit(1);
	}

	char *input_filename = argv[1];
	char *output_filename = argv[2];
	int64_t start_time = atol(argv[3]);
	int64_t end_time = atol(argv[4]);

	tvie_assert(start_time < end_time);

	av_register_all();

	AVFormatParameters params, *ap = &params;
	memset(ap, 0, sizeof(*ap));
	ap->prealloced_context = 1;

	input_context = avformat_alloc_context();
	int ret = av_open_input_file(&input_context, input_filename, NULL, 0, ap);
	if (ret != 0) {
		tvie_error("%s error: Could not open input file: %d", argv[0], ret);
		exit(1);
	}

	if (av_find_stream_info(input_context) < 0) {
		tvie_error("%s error: Could not read stream information", argv[0]);
		exit(1);
	}

	input_context->flags |= AVFMT_FLAG_GENPTS;
	input_context->debug = FF_FDEBUG_TS;

	tvie_info("-----input file info-----");
	dump_format(input_context, 0, input_context->iformat->name, 0);

	unsigned int i = 0;
	for(; i < input_context->nb_streams; ++i) {
		if (input_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
			audio_index = i;
			audio_decoder_ctx = input_context->streams[i]->codec;
		}
		if (input_context->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			video_index = i;
			video_decoder_ctx = input_context->streams[i]->codec;
		}
	}

	AVCodec *video_decoder =  avcodec_find_decoder(video_decoder_ctx->codec_id);
	video_decoder_ctx->get_buffer = our_get_buffer;
	video_decoder_ctx->release_buffer = our_release_buffer;
	video_decoder_ctx->debug |= FF_DEBUG_PICT_INFO;
	avcodec_open(video_decoder_ctx, video_decoder);
	tvie_assert(audio_decoder_ctx != NULL);
	tvie_assert(video_decoder_ctx != NULL);

	// setup output stream context.
	AVOutputFormat *output_format = NULL;
	output_format = av_guess_format("mp4", NULL, NULL);
	if (!output_format) {
		tvie_error("Slicer error: Could not find mpeg4 muxer");
		exit(1);
	}

	output_context = avformat_alloc_context();
	if (!output_context) {
		tvie_error("Slicer error: Could not allocated output context");
		exit(1);
	}
	output_context->oformat = output_format;


	// setup video encoder.
	video_stream = av_new_stream(output_context, output_context->nb_streams);
	video_stream->time_base = input_context->streams[video_index]->time_base;
	setup_video_codec();
	if (avcodec_open(video_enc_ctx, video_encoder) < 0) {
		tvie_error("Slicer error: failed to open video enc");
		exit(-1);
	}

	tvie_info("encoder extradata");
	if(video_enc_ctx->extradata)
		tvie_hexdump(video_enc_ctx->extradata, video_enc_ctx->extradata_size);

	ByteIOContext *pb;
	uint8_t *buf;
	int size;

	ret = url_open_dyn_buf(&pb);
	if (ret < 0)
		return ret;

	ff_isom_write_avcc(pb, video_enc_ctx->extradata, video_enc_ctx->extradata_size);
	size = url_close_dyn_buf(pb, &buf);

	tvie_info("decoder extradata");
	if(video_decoder_ctx->extradata)
		tvie_hexdump(video_decoder_ctx->extradata, video_decoder_ctx->extradata_size);

	tvie_info("encoder extradata");
	uint64_t extra_size;
	//extra_size = video_decoder_ctx->extradata_size +  FF_INPUT_BUFFER_PADDING_SIZE;
	extra_size = video_decoder_ctx->extradata_size + size + FF_INPUT_BUFFER_PADDING_SIZE;
	//extra_size = size + FF_INPUT_BUFFER_PADDING_SIZE;
	video_enc_ctx->extradata = av_mallocz(extra_size);
	memcpy(video_enc_ctx->extradata, buf, size);
	memcpy(video_enc_ctx->extradata + size , video_decoder_ctx->extradata, video_decoder_ctx->extradata_size);
	video_enc_ctx->extradata_size = size + video_decoder_ctx->extradata_size;
	//video_enc_ctx->extradata_size = video_decoder_ctx->extradata_size;

	if(video_enc_ctx->extradata)
		tvie_hexdump(video_enc_ctx->extradata, video_enc_ctx->extradata_size);
	// setup audio encoder.
	audio_stream = av_new_stream(output_context, output_context->nb_streams);
	avcodec_get_context_defaults2(audio_stream->codec, CODEC_TYPE_AUDIO);
	audio_enc_ctx = audio_stream->codec;
	audio_encoder = avcodec_find_encoder(audio_decoder_ctx->codec_id);

	audio_enc_ctx->codec_type = CODEC_TYPE_AUDIO;
	audio_enc_ctx->codec_id = audio_encoder->id;
	audio_enc_ctx->bit_rate = audio_decoder_ctx->bit_rate;
	audio_enc_ctx->bit_rate_tolerance = audio_decoder_ctx->bit_rate_tolerance;
	audio_enc_ctx->channels = audio_decoder_ctx->channels;
	audio_enc_ctx->channel_layout = audio_decoder_ctx->channel_layout;
	audio_enc_ctx->sample_rate = audio_decoder_ctx->sample_rate;
	audio_enc_ctx->time_base.num = audio_decoder_ctx->time_base.num;
	audio_enc_ctx->time_base.den = audio_decoder_ctx->time_base.den;
	audio_enc_ctx->frame_size = audio_decoder_ctx->frame_size;
	audio_enc_ctx->block_align = audio_decoder_ctx->block_align;
	if(audio_enc_ctx->block_align == 1 && audio_enc_ctx->codec_id == CODEC_ID_MP3)
		audio_enc_ctx->block_align= 0;
	if(audio_enc_ctx->codec_id == CODEC_ID_AC3)
		audio_enc_ctx->block_align= 0;

	audio_enc_ctx->flags = audio_decoder_ctx->flags;
	if(output_context->oformat->flags & AVFMT_GLOBALHEADER)
		audio_enc_ctx->flags |= CODEC_FLAG_GLOBAL_HEADER;

	extra_size = (uint64_t)audio_decoder_ctx->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE;
	audio_enc_ctx->extradata = av_mallocz(extra_size);
	memcpy(audio_enc_ctx->extradata, audio_decoder_ctx->extradata, audio_decoder_ctx->extradata_size);
	audio_enc_ctx->extradata_size = audio_decoder_ctx->extradata_size;
	if (av_set_parameters(output_context, NULL) < 0) {
		tvie_error("Slicer error: Invalid output format parameters\n");
		exit(1);
	}

	tvie_info("-----output file info-----");
	dump_format(output_context, 0, output_filename, 1);
	if (url_fopen(&output_context->pb, output_filename, URL_WRONLY) < 0) {
		tvie_error("Slicer error: Could not open '%s'", output_filename);
		exit(1);
	}

	if (av_write_header(output_context)) {
		tvie_error("Slicer error: Could not write mpegts header to first output file");
		exit(1);
	}

	int key_frame_number = 0;

	int64_t end_part_start_time;
	if (end_time != 0) {
		ret = av_seek_frame(input_context, -1, end_time, AVSEEK_FLAG_BACKWARD);
		if (ret < 0) {
			fprintf(stderr, "%s: could not seek to position %0.3f\n", input_filename, (double)end_time / AV_TIME_BASE);
		}
	}

	while(1){
		AVPacket packet;
		av_read_frame(input_context, &packet);
		if (packet.stream_index == video_index) {
			if (packet.flags & PKT_FLAG_KEY){
				end_part_start_time = packet.pts;
			}

			//tvie_info("seek packet: pts: %"PRId64", dts:%"PRId64", size:%d", packet.pts, packet.dts, packet.size);
			break;
		}
	}

	int64_t start_part_start_time = AV_NOPTS_VALUE;
	ret = av_seek_frame(input_context, -1, start_time, AVSEEK_FLAG_BACKWARD);
	if (ret < 0) {
		fprintf(stderr, "%s: could not seek to position %0.3f\n", input_filename, (double)start_time / AV_TIME_BASE);
	}

	while(1){
		AVPacket packet;
		av_read_frame(input_context, &packet);
		if (packet.stream_index == video_index) {
			if (packet.flags & PKT_FLAG_KEY){
				if(start_part_start_time == AV_NOPTS_VALUE)
					start_part_start_time = packet.pts;
				key_frame_number++;
			}

			//tvie_info("seek packet: pts: %"PRId64", dts:%"PRId64", size:%d", packet.pts, packet.dts, packet.size);
			if(packet.pts == end_part_start_time || key_frame_number >= 3) {
				break;
			}
		}
	}
	tvie_info("key frame number: %d", key_frame_number);
	ret = av_seek_frame(input_context, -1, start_time, AVSEEK_FLAG_BACKWARD);
	if (ret < 0) {
		fprintf(stderr, "%s: could not seek to position %0.3f\n", input_filename, (double)start_time / AV_TIME_BASE);
	}


	int audio_start_time = av_rescale_q(start_time, AV_TIME_BASE_Q, input_context->streams[audio_index]->time_base);
	start_time = av_rescale_q(start_time, AV_TIME_BASE_Q, input_context->streams[video_index]->time_base);
	end_time = av_rescale_q(end_time, AV_TIME_BASE_Q, input_context->streams[video_index]->time_base);
	tvie_info("start time: %d, end time: %d, start_part_start: %d, end_part_start: %d", 
			start_time, end_time, start_part_start_time, end_part_start_time);
	// 1. a. I ----- S ----- E 
	// 	  b. S(I) ----- E					
	// 2. a. I ----- S ----- I ----- E 
	//    b. S(I) ----- I ----- E
	//    c. I ----- S ----- E(I) 
	//    d. S(I) ----- E(I)
	// 3. a. I ----- S ----- I ----- I ----- E 
	//    b. S(I) ----- I ----- I ----- E 
	//    c. I ----- S ----- I ----- E(I) 
	//    d. S(I) ----- I ----- (E)I
	//
	// 2d 3d: copy only
	// 1a 1b 2a 2c: encode only
	// 2b 3b: copy + encode
	// 3a: encode + copy + encode
	// 3c: encode + copy 

	int slice_mode = 0;
	if(start_time == start_part_start_time){ // start time frame is key frame.

		if(end_time == end_part_start_time){ // end time frame is key frame.
			// 2d 3d
			slice_mode = MODE_COPY_ONLY; 
		}
		else { // end_time != end_part_start_time
			if(key_frame_number == 1){
				// 1b
				slice_mode = MODE_ENCODE_ONLY;
			}
			else { // (key_frame_number > 1)
				// 2b 3b
				slice_mode = MODE_COPY_ENCODE;
			}
		}
	} else {
		if(end_time == end_part_start_time){ // end time frame is key frame.
			if(key_frame_number == 2){
				// 2c
				slice_mode = MODE_ENCODE_ONLY;
			}
			else if(key_frame_number == 3){
				// 3c
				slice_mode = MODE_ENCODE_COPY;
			}
		} else {
			if(key_frame_number <= 2){
				// 1a 2a
				slice_mode = MODE_ENCODE_ONLY;
			} else {
				// 3a
				slice_mode = MODE_ENCODE_COPY_ENCODE;
			}
		}

	}
	tvie_info("slice_mode; %d", slice_mode);

	AVFrame picture;
	enc_buffer = malloc(TVIE_ENC_BUFFER_SIZE);
	if (enc_buffer == NULL) {
		tvie_error("malloc for video enc_buffer failed");
		exit(-1);
	}

	for (i = 0; i < MAX_REORDER_DELAY + 1; i++)
		pts_buffer[i] = AV_NOPTS_VALUE;

	int read_done;
	int do_copy_data = 0;
	do {
		AVPacket packet;
		read_done = av_read_frame(input_context, &packet);
		if (read_done < 0) {
			break;
		}

		if (packet.stream_index == video_index) {
			if (packet.flags & PKT_FLAG_KEY) {
				tvie_info("key frame!");
			}
			tvie_info("video pkt: pts: %"PRId64", dts:%"PRId64", size:%d", packet.pts, packet.dts, packet.size);

			if(slice_mode == MODE_COPY_ONLY) {
				if(packet.pts > end_time){
						av_free_packet(&packet);
					break;
				}
				output_video_packet(&packet, output_context);
			}
			else if(slice_mode == MODE_ENCODE_ONLY) {

				if(packet.pts < start_time){
					decode_video_packet(&packet, &picture);
					av_free_packet(&packet);
					continue;
				} else if(packet.pts > end_time){
						av_free_packet(&packet);

					flush_decoder_data();
					flush_encoder_data();

					break;                    
				}	
				if(!output_packet_encode(&packet))
					output_video_packet(&packet, output_context);
				else
					av_free_packet(&packet);
			}	
			else if(slice_mode == MODE_ENCODE_COPY) {

				if(do_copy_data == 0){
					if(packet.pts < start_time){
						decode_video_packet(&packet, &picture);
						av_free_packet(&packet);
						continue;
					} else if((packet.flags & PKT_FLAG_KEY) && 
							packet.pts > start_part_start_time){

						flush_decoder_data();
						flush_encoder_data();
						output_video_packet(&packet, output_context);
						do_copy_data = 1;
					} else {
						if(!output_packet_encode(&packet))
							output_video_packet(&packet, output_context);
						else
							av_free_packet(&packet);
					}
				} else {
					if(packet.pts > end_time){
						av_free_packet(&packet);
						break;
					}
					output_video_packet(&packet, output_context);
				}

			}
			else if(slice_mode == MODE_COPY_ENCODE) {
				if(packet.pts < end_part_start_time){
					output_video_packet(&packet, output_context);
				} else {
					if(packet.pts > end_time){
						av_free_packet(&packet);

						flush_decoder_data();
						flush_encoder_data();

						break;
					} 

					if(!output_packet_encode(&packet))
						output_video_packet(&packet, output_context);
					else
						av_free_packet(&packet);
				}
			}
			else if(slice_mode == MODE_ENCODE_COPY_ENCODE) {
				if(do_copy_data == 0){
					if(packet.pts < start_time){
						tvie_info("MODE_ECE: part 1 decode but discard.");
						decode_video_packet(&packet, &picture);
						av_free_packet(&packet);
						continue;
					} else if((packet.flags & PKT_FLAG_KEY) &&
							packet.pts > start_part_start_time){

						tvie_info("MODE_ECE: part 1 flush decoder and encoder.");
						flush_decoder_data();
						flush_encoder_data();
						output_video_packet(&packet, output_context);
						do_copy_data = 1;
					} else {
						tvie_info("MODE_ECE: part 1 decode and encode.");
						if(!output_packet_encode(&packet))
							output_video_packet(&packet, output_context);
						else
							av_free_packet(&packet);
					}
				} else if(packet.pts < end_part_start_time){
					tvie_info("MODE_ECE: part 2 copy data.");
					output_video_packet(&packet, output_context);
				} else {
					static int reset_encoder = 1;
					if(reset_encoder){
						avcodec_close(video_decoder_ctx);
						avcodec_open(video_decoder_ctx, video_decoder);
						tvie_info("decoder extradata");
						if(video_decoder_ctx->extradata)
							tvie_hexdump(video_decoder_ctx->extradata, video_decoder_ctx->extradata_size);
						avcodec_close(video_enc_ctx);
						setup_video_codec();
						avcodec_open(video_enc_ctx, video_encoder);
						uint64_t extra_size;
						extra_size = video_decoder_ctx->extradata_size + FF_INPUT_BUFFER_PADDING_SIZE;
						video_enc_ctx->extradata = av_mallocz(extra_size);
						memcpy(video_enc_ctx->extradata, video_decoder_ctx->extradata, video_decoder_ctx->extradata_size);
						video_enc_ctx->extradata_size = video_decoder_ctx->extradata_size;
						tvie_info("encoder extradata");
						if(video_enc_ctx->extradata)
							tvie_hexdump(video_enc_ctx->extradata, video_enc_ctx->extradata_size);

						reset_encoder = 0;
					}

					if(packet.pts > end_time){
						av_free_packet(&packet);
						tvie_info("MODE_ECE: part 3 flush decoder and encoder.");
						flush_decoder_data();
						flush_encoder_data();
						break;

					} else {
						tvie_info("MODE_ECE: part 3 decode and encode.");
						if(!output_packet_encode(&packet))
							output_video_packet(&packet, output_context);
						else
							av_free_packet(&packet);
					}	
				}
			}

		} // end of if (packet.stream_index == video_index)
		else if (packet.stream_index == audio_index){

			if(packet.pts < audio_start_time){
				av_free_packet(&packet);
				continue;
			}
			packet.stream_index = 1;
			tvie_info("audio pkt: pts: %"PRId64", dts:%"PRId64", size:%d", packet.pts, packet.dts, packet.size);
			ret = av_interleaved_write_frame(output_context, &packet);
			if (ret < 0) {
				tvie_info("tvie_transcoding_slicer: Could not write frame of stream: %d", ret);
			}
			else if (ret > 0) {
				tvie_info("tvie_transcoding_slicer: End of stream requested");
				av_free_packet(&packet);
				break;
			}
		} // end of else (packet.stream_index == audio_index)

		av_free_packet(&packet);
	} while (!read_done);

	av_write_trailer(output_context);

	if (video_index >= 0) {
		avcodec_close(video_stream->codec);
	}

	for(i = 0; i < output_context->nb_streams; i++) {
		av_freep(&output_context->streams[i]->codec);
		av_freep(&output_context->streams[i]);
	}

	url_fclose(output_context->pb);
	av_free(output_context);

	return 0;
}

