#pragma once
#include <iostream>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavdevice/avdevice.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}
#include <string>
#include <thread>
#include <mutex>
#if _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#define	R_OK	4		/* Test for read permission.  */
#define	W_OK	2		/* Test for write permission.  */
#define	X_OK	1		/* Test for execute permission.  */
#define	F_OK	0		/* Test for existence.  */

typedef struct VideoData {
    AVFormatContext* fmt_ctx;
    int stream_idx;
    AVStream* video_stream;
    AVCodecContext* codec_ctx;
    AVCodecParameters* codec_ctxpar;
    const AVCodec* decoder;
    AVPacket* packet;
    struct SwsContext* conv_ctx_yuv;
    struct SwsContext* conv_ctx;
    enum AVHWDeviceType type;
    struct ReadBuffer* ReadBufferHead;

    int RGBFramesize;
    int YUVFramesize;
    bool replay=false;
}VideoData;

typedef struct ReadBuffer {
    int index = 0;
    int current_state = 0; // 0:unready 1:codec  2:trans   3:ready
    AVFrame* codec_buffer = NULL;
    AVFrame* frame_buffer = NULL;
    AVFrame* tmp_frame_nv12 = NULL;
    AVFrame* tmp_frame_yuv = NULL;
    uint8_t* internal_buffer;
    uint8_t* internal_buffer_yuv;
    struct ReadBuffer* next_buffer;
}ReadBuffer;


typedef struct VideoList{
    int idx  ;
    int height;
    int width;
    VideoData *data;
    VideoList* next;
}VideoList;


static AVBufferRef* hw_device_ctx;
static enum AVPixelFormat hw_pix_fmt;  // IF SUPPORT HARDWARE DECODE
static enum AVPixelFormat get_hw_format(AVCodecContext* ctx,const enum AVPixelFormat* pix_fmts)
{
    const enum AVPixelFormat* p;

    for (p = pix_fmts; *p != -1; p++) {
        if (*p == hw_pix_fmt)
            return *p;
    }

    fprintf(stderr, "Failed to get HW surface format.\n");
    return AV_PIX_FMT_NONE;
}


class Decoder {

public :

	Decoder() {
		avdevice_register_all();
		avformat_network_init();
        VideoListHead = NULL;
	}

    const int  add_video_by_path(std::string path);
    void delete_video_by_id(int id);
    void show_video_list();
    void parse(int video_id);
    uint8_t* get_current_frame_data( );

    int test_num = 0;
    bool decoding = false;
   

private:

    int VideoIDGenerator = 0;
    VideoList* VideoListHead;
   int ReadBufferNum = 4;
   ReadBuffer* current_buffer;
   uint8_t* current_frame_data;

   VideoList* create_Video_buffer(int idx, VideoData *data) {
       VideoList* newBuffer = (VideoList*)malloc(sizeof(VideoList));
       if (newBuffer == NULL) {
           printf("Memory allocation failed\n");
           exit(1);
       }
       newBuffer->idx = idx;
       newBuffer->data = data;
       newBuffer->next = NULL;
       return newBuffer;
   }
   void AddVideoBuffer(VideoList** headRef, int idx, VideoData *data);
   void ClearVideoBuffer(VideoList* Buffer);
   void ClearAllVideoBuffer(VideoList** headRef);

   void ReadBufferThread(VideoData* data);
   void TransferDataToRGBThread(VideoData* data);
   static int hw_decoder_init(AVCodecContext* ctx, const enum AVHWDeviceType type)
   {
       int err = 0;

       if ((err = av_hwdevice_ctx_create(&hw_device_ctx, type,
           NULL, NULL, 0)) < 0) {
           fprintf(stderr, "Failed to create specified HW device.\n");
           return err;
       }
       ctx->hw_device_ctx = av_buffer_ref(hw_device_ctx);

       return err;
   }


    void  initial_video_data(VideoData* data) {
        data->packet = NULL;
        data->fmt_ctx = NULL;
        data->stream_idx = -1;
        data->video_stream = NULL;
        data->codec_ctx = NULL;
        data->codec_ctxpar = NULL;
        data->decoder = NULL;
        data->conv_ctx = NULL;
        data->conv_ctx_yuv = NULL;
    }
    bool checkFile(const char* VideoPath, VideoData* data) {

        data->type = av_hwdevice_find_type_by_name("cuda");

        if (data->type == AV_HWDEVICE_TYPE_NONE) {
            fprintf(stderr, "Device type %s is not supported.\n", "cuda");
            fprintf(stderr, "Available device types:");
            while ((data->type = av_hwdevice_iterate_types(data->type)) != AV_HWDEVICE_TYPE_NONE)
            fprintf(stderr, " %s", av_hwdevice_get_type_name(data->type));
            fprintf(stderr, "\n");
            return false;
        }

        if (avformat_open_input(&data->fmt_ctx, VideoPath, NULL, NULL) < 0) {
            std::cout << "failed to load file" << std::endl;
            clearVideoData(data);
            return false;
        }

        if (avformat_find_stream_info(data->fmt_ctx, NULL) < 0) {
            std::cout << "fail to get stream info" << std::endl;
            clearVideoData(data);
            return false;
        }
        av_dump_format(data->fmt_ctx, 0, VideoPath, 0);

        for (unsigned int i = 0; i < data->fmt_ctx->nb_streams; ++i) {

            if (data->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                data->stream_idx = i;
                break;
            }
        }
        if (data->stream_idx == -1) {
            std::cout << "fail to find video stream" << std::endl;
            clearVideoData(data);
            return false;
        }
        return true;
    }
    bool set_decoder(VideoData* data);
    void clearVideoData(VideoData* data);

    bool fileExists(const std::string& path) {
        bool ret = false;
        if ((_access(path.c_str(), F_OK)) != -1) {
            ret = true;
        }
        return ret;
    }

};