#ifndef _LSP_H_
#define _LSP_H_

#include <sys/types.h>



#define MAX_PATH_LEN    1024
#define TIME_BASE   1000
 


/*** structs ***/
enum audio_sample_format_type{
    AUDIO_SAMPLE_FORMAT_TYPE_NONE   = -1,   // default
    AUDIO_SAMPLE_FORMAT_TYPE_U8     = 0,    // unsigned 8 bits
    AUDIO_SAMPLE_FORMAT_TYPE_S16    = 1,    // signed 16 bits
    AUDIO_SAMPLE_FORMAT_TYPE_S32    = 2,    // signed 32 bits
};

// error_code
#define     ERROR_CODE_OK          0
#define     ERROR_CODE_EINVAL     -5   /**< Invalid input arguments (failure). */
#define     ERROR_CODE_ENOMEM     -4   /**< No memory available (failure). */
#define     ERROR_CODE_EIO        -3   /**< An IO error occured (failure). */
#define     ERROR_CODE_ENOTIMPL   -2   /**< Functionality not implemented (failure). */
#define     ERROR_CODE_EFAIL      -1   /**< General failure code (failure). */
#define     ERROR_CODE_EOK         0   /**< General success code (success). */
#define     ERROR_CODE_EFLUSH      1   /**< The command was flushed (success). */
#define     ERROR_CODE_EPRIME      2   /**< The command was primed (success). */
#define     ERROR_CODE_EFIRSTFIELD 3   /**< Only the first field was processed (success)*/
#define     ERROR_CODE_EBITERROR   4   /**< There was a non fatal bit error (success). */
#define     ERROR_CODE_ETIMEOUT    5   /**< The operation was timed out (success). */
#define     ERROR_CODE_EEOF        6   /**< The operation reached end of file */
#define     ERROR_CODE_EAGAIN      7   /**< The command needs to be rerun (success). */

#define     ERROR_CODE_MALLOC_ERROR                (-10)
#define     ERROR_CODE_NULL_POINTER                (-11)


#define     ERROR_CODE_INPUT_ARGS_ERROR            (-100)
#define     ERROR_CODE_MEMORY_ALLOC_ERROR          (-101)
#define     ERROR_CODE_NO_OUTPUT                   (-102)
#define     ERROR_CODE_NO_INPUT                    (-103)
#define     ERROR_CODE_DATA_TYPE_ERROR             (-104)
#define     ERROR_CODE_PACK_RAW_VIDEO_ERROR        (-105)
#define     ERROR_CODE_PACK_RAW_AUDIO_ERROR        (-106)
#define     ERROR_CODE_PACK_ENC_VIDEO_ERROR        (-107)
#define     ERROR_CODE_PACK_ENC_AUDIO_ERROR        (-108)
#define     ERROR_CODE_NOT_SUPPORT_VIDEO_RAW_TYPE  (-109)
#define     ERROR_CODE_NOT_SUPPORT_VIDEO_ENC_TYPE  (-110)
#define     ERROR_CODE_NOT_SUPPORT_AUDIO_RAW_TYPE  (-111)
#define     ERROR_CODE_NOT_SUPPORT_AUDIO_ENC_TYPE  (-112)



#define     ERROR_CODE_VIDEO_WIDTH_ERROR           (-200)
#define     ERROR_CODE_VIDEO_HEIGHT_ERROR          (-201)
#define     ERROR_CODE_VIDEO_FRAME_RATE_ERROR      (-202)
#define     ERROR_CODE_VIDEO_CODEC_ERROR           (-203)
#define     ERROR_CODE_VIDEO_CTX_ERROR             (-204)
#define     ERROR_CODE_VIDEO_CODEC_OPEN_ERROR      (-205)



#define     ERROR_CODE_AUDIO_CHANNEL_ERROR         (-300)
#define     ERROR_CODE_AUDIO_SAMPLE_RATE_ERROR     (-301)
#define     ERROR_CODE_AUDIO_SAMPLE_FMT_ERROR      (-302)
#define     ERROR_CODE_AUDIO_CODEC_ERROR           (-303)
#define     ERROR_CODE_AUDIO_CTX_ERROR             (-304)
#define     ERROR_CODE_AUDIO_CODEC_OPEN_ERROR      (-305)
#define     ERROR_CODE_AUDIO_DATA_FILL_ERROR       (-306)
#define     ERROR_CODE_AUDIO_ENCODE_ERROR          (-307)


#define     ERROR_CODE_OUT_CTX_CREATE_ERROR        (-400)
#define     ERROR_CODE_OUT_STREAM_ERROR            (-401)
#define     ERROR_CODE_OUT_OPEN_FILE_ERROR         (-402)


#define FALSE 0
#define TRUE  1


typedef struct Lsp_args{
    char stream_src_path[MAX_PATH_LEN]; // "must be set ! it used to get video encoder info"
    char stream_out_path[MAX_PATH_LEN]; // "must be set !"

    int have_video_stream; // "must be set !  set > 0 , means you will push video data"
    int have_audio_stream; // "must be set !  set > 0 , means you will push audio data"

    // video raw data, the video true data params
    int i_width;   // range 176 to 1920, "must be set !"
    int i_height;  // range 144 to 1080, "must be set !"
    int i_pix_fmt; // now only support yv12(420), no need to set
    int i_frame_rate; // range 1 to 30, "must be set !"
    
    // audio raw data, the audio true data params
    int i_channels; // range 1 to 2, "must be set !"
    int i_sample_rate; // "must be set !", support 44100,22050
    int i_sample_fmt; // "must be set !", use enum xchannel_audio_sample_format_type  

    // video enc params, the params which you want to encode
    int vbit_rate;  // range 64000 to 1000000, default 100000bps
    int o_width;    // range 10 to 1920, out of range will be set same as input
    int o_height;   // range 10 to 1080, out of range will be set same as input
    int o_pix_fmt;  // now only support yv12(420), no need to set
    int o_frame_rate;  // range 1 to 30, out of range will be set same as input
    int gop;        // range 25 to 250, default 50
    int coder_type; // 
    
    // audio enc params, the params which you want to encode
    int abit_rate;   //sugest 48000, 64000, default 48000, range 32k to 196k
    int o_channels;  // range 1 to 2, out of range will be set same as input
    int o_sample_rate; // suggest 44100, out of range will be set same as input


    int  aextradata_size; 
    int  vextradata_size; // used for input data is encoded type
    char aextradata[128];
    char vextradata[256];

}Lsp_args;

enum Lsp_data_type{
    DATA_TYPE_NONE,
    DATA_TYPE_ENC_VIDEO,  // encoded video (h264 nals)
    DATA_TYPE_ENC_AUDIO,  // encoded audio
    DATA_TYPE_FILE,// file path
};


typedef struct Lsp_data{
    int type; //video, audio, use enum Lsp_data_type;
    int size; // data size
    int64_t timebase;
    int64_t pts;
    int64_t dts;
    void * data; // the pointer which point to data space
    int is_key;
}Lsp_data;


typedef void * Lsp_handle;


struct Lsp_status{

};


/*** functions ***/
/**
 * Create the Lsp object.
 * @return          not NULL if OK,  NULL if error
 */
Lsp_handle Lsp_create(void);

/**
 * Init the Lsp object by Lsp_args.
 * @param handle       the pointer point to Lsp_handle which will be inited
 * @param args      the pointer point to Lsp_args that contained params for init
 * @return          0 if OK,  <0 if some error
 */
int Lsp_init(Lsp_handle handle, struct Lsp_args * args);

/**
 * Push the data.
 * @param handle       the pointer point to Lsp_handle
 * @param data      the pointer point to Lsp_data that will be pushed
 * @return          0 if OK,  <0 if some error
 */
int Lsp_push(Lsp_handle handle, struct Lsp_data * data);

/**
 * Get the Lsp status info.
 * @param handle       the pointer point to Lsp_handle
 * @param status    the pointer point to Lsp_status that will be writed status info
 * @return          0 if OK,  <0 if some error
 */
int Lsp_status(Lsp_handle handle, struct Lsp_status * status);

/**
 * Release the Lsp object.
 * @param handle       the pointer point to Lsp_handle
 * @return          0 if OK,  <0 if some error
 */
int Lsp_release(Lsp_handle * handle);



#endif
