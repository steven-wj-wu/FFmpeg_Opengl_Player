#include "Decoder.h"
#define Frame_Buffer_State_Extract 0
#define Frame_Buffer_State_Transform 1
#define Frame_Buffer_State_Read 2

//PUBLIC  FUNCTION------------------------------------

const int Decoder::add_video_by_path(std::string path) {
    //ceck file 
    if (fileExists(path)) {
        VideoData* new_data = new VideoData();
        initial_video_data(new_data);
        const char* path_c = path.c_str();
        if (checkFile(path_c, new_data)) {
            if (set_decoder(new_data)) {
                AddVideoBuffer(&VideoListHead, VideoIDGenerator, new_data);
                return VideoIDGenerator-1;
            }
        }
    }
    else {
        printf("No Video File Found(Decoder)");
    }
}

void Decoder::delete_video_by_id(int id) {

    if (VideoListHead == NULL) {
        printf("List is Empty!\n");
        return;
    }
    else {
        VideoList* current = VideoListHead;
        VideoList* previous = NULL;
        //IF IN HEAD NODE
         if (current->idx == id) {    
             VideoListHead = current->next;
             ClearVideoBuffer(current);
             printf("Delete Success!\n");
             return;
         }
        //WHILE SEARCH
         else {
             if (current->next == nullptr) {
                 printf("ID not flound in list!\n");
                 return;
             }
             else {
                 while (current != nullptr) {
                     previous = current;
                     current = current->next;
                     if (current->idx == id) {
                         previous->next = current->next;
                         ClearVideoBuffer(current);
                         printf("Delete Success!\n");
                         return;
                     }
                 }
                 printf("ID not flound in list!\n");
                 return;               
             }
         }   
    }
    
   

}

void Decoder::show_video_list() {

    if (VideoListHead == NULL) {
        printf("No Video!\n");
    }
    else {
        VideoList* current = VideoListHead;
        printf("video_id: %d\n", current->idx);

        while (current->next != NULL) {
            current = current->next;
            printf("video_id: %d\n", current->idx);
        }
    }
   
}

void Decoder::parse(int video_id) {


    VideoList* current = VideoListHead;
    while (current!= nullptr) {
        if (current->idx == video_id)
            break;
        current = current->next;
    }


    //open dcoder
    if (avcodec_open2(current->data->codec_ctx, current->data->decoder, NULL) < 0) {
        std::cout << "fail to  open decoder" << std::endl;
        return;
    }


    current->data->packet = (AVPacket*)av_malloc(sizeof(AVPacket));

    current->height = current->data->codec_ctx->height;
    current->width = current->data->codec_ctx->width;
    current->data->RGBFramesize = av_image_get_buffer_size(AV_PIX_FMT_BGR24, current->width, current->height, 1);
    current->data->YUVFramesize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, current->width, current->height, 1);


    if (current->data->ReadBufferHead == nullptr) {
        current->data->ReadBufferHead = new ReadBuffer();
         current->data->ReadBufferHead->index = 0;
         current->data->ReadBufferHead->frame_buffer = av_frame_alloc();
         current->data->ReadBufferHead->codec_buffer = av_frame_alloc();
         current->data->ReadBufferHead->tmp_frame_nv12 = av_frame_alloc();
         current->data->ReadBufferHead->tmp_frame_yuv = av_frame_alloc();
         current->data->ReadBufferHead->internal_buffer = (uint8_t*)av_malloc(current->data->RGBFramesize * sizeof(uint8_t));
         av_image_fill_arrays(current->data->ReadBufferHead->frame_buffer->data, current->data->ReadBufferHead->frame_buffer->linesize, current->data->ReadBufferHead->internal_buffer, AV_PIX_FMT_BGR24, current->width, current->height, 1);
         current->data->ReadBufferHead->internal_buffer_yuv = (uint8_t*)av_malloc(current->data->YUVFramesize * sizeof(uint8_t));
         av_image_fill_arrays(current->data->ReadBufferHead->tmp_frame_yuv->data, current->data->ReadBufferHead->tmp_frame_yuv->linesize, current->data->ReadBufferHead->internal_buffer_yuv, AV_PIX_FMT_YUV420P, current->width, current->height, 1);
         current->data->ReadBufferHead->next_buffer = new ReadBuffer();
         ReadBuffer* ptr = current->data->ReadBufferHead;

         for (int i = 1; i < ReadBufferNum-1; i++) {
            ptr->next_buffer = new ReadBuffer();
            ptr = ptr->next_buffer;
            ptr->index = i;
            ptr->frame_buffer = av_frame_alloc();
            ptr->codec_buffer= av_frame_alloc();
            ptr->tmp_frame_nv12= av_frame_alloc();
            ptr->tmp_frame_yuv= av_frame_alloc();
            ptr->internal_buffer = (uint8_t*)av_malloc(current->data->RGBFramesize * sizeof(uint8_t));
            av_image_fill_arrays(ptr->frame_buffer->data, ptr->frame_buffer->linesize,  ptr->internal_buffer, AV_PIX_FMT_BGR24, current->width, current->height, 1);
            ptr->internal_buffer_yuv = (uint8_t*)av_malloc(current->data->YUVFramesize * sizeof(uint8_t));
            av_image_fill_arrays(ptr->tmp_frame_yuv->data, ptr->tmp_frame_yuv->linesize, ptr->internal_buffer_yuv, AV_PIX_FMT_YUV420P, current->width, current->height, 1);
        }
       ptr->next_buffer  = current->data->ReadBufferHead;
         //ÀY§À¦ê±µ
    }

    decoding = true;
    current->data->replay = false;
    std::thread reader(&Decoder::ReadBufferThread, this,current->data);
    std::thread transfer(&Decoder::TransferDataToRGBThread, this, current->data);
    reader.detach();
    transfer.detach();
}


void Decoder::free_buffer(int video_id) {

    VideoList* current = VideoListHead;
    while (current != nullptr) {
        if (current->idx == video_id)
            break;
        current = current->next;
    }
    if (current != nullptr) {
        ClearVideoBuffer(current);
    }
    else {
        printf("Free Buffer Failed: No video id exisit\n");
    }
   
}

uint8_t* Decoder::get_current_frame_data( ) {

        if (current_buffer == nullptr) {
            printf("No Data in Readbuffer\n");
            return nullptr;
        }
        else {      
            if (current_buffer->current_state == Frame_Buffer_State_Read) {
                current_frame_data = current_buffer->frame_buffer->data[0];
                current_buffer->current_state = Frame_Buffer_State_Extract;
                current_buffer = current_buffer->next_buffer;
                return current_frame_data;
           }
        }
}

void Decoder::ReadBufferThread(VideoData *data) {

    ReadBuffer* ptr = data->ReadBufferHead;

    if (ptr == nullptr) {
        printf("buffer error\n");
        return;
    }


    if (!data->conv_ctx_yuv) {
        data->conv_ctx_yuv = sws_getContext(data->codec_ctx->width,
            data->codec_ctx->height, AV_PIX_FMT_NV12,
            data->codec_ctx->width, data->codec_ctx->height, AV_PIX_FMT_YUV420P, NULL, NULL, NULL, NULL);
    }

    if (!data->conv_ctx) {
        data->conv_ctx = sws_getContext(data->codec_ctx->width,
            data->codec_ctx->height, AV_PIX_FMT_YUV420P,
            data->codec_ctx->width, data->codec_ctx->height, AV_PIX_FMT_BGR24, NULL, NULL, NULL, NULL);
    }

   
    current_buffer = data->ReadBufferHead;
    current_frame_data = (uint8_t*)malloc(sizeof(uint8_t) * data->RGBFramesize);
    while (decoding) {

                while (1) {
                    //add replay here 
                    if (ptr->current_state== Frame_Buffer_State_Extract) {//not read buffer yet

                        if (!(av_read_frame(data->fmt_ctx, data->packet) >= 0)) {
                            printf("read frame failed \n");
                            break;
                        }

                        if (data->packet->stream_index == data->stream_idx) {

                            if (avcodec_send_packet(data->codec_ctx, data->packet) < 0) {
                                printf("ask packet failed \n");
                                break;
                            }
                            if (!(avcodec_receive_frame(data->codec_ctx, ptr->codec_buffer) < 0)) {
                                ptr->current_state = Frame_Buffer_State_Transform;
                                ptr = ptr->next_buffer;
                            }
                        }
                    }
                    av_packet_unref(data->packet);                 
                }

                if (data->replay) {
                    av_seek_frame(data->fmt_ctx, data->stream_idx, 0, 8);
                }
                else {
                    decoding = false;
                    break;
                }
        }


}

void Decoder::TransferDataToRGBThread(VideoData* data) {
    
    ReadBuffer* ptr = data->ReadBufferHead;
    int ret;

    while (decoding) {
        
        if (ptr->codec_buffer == NULL)
        {
            ptr->tmp_frame_yuv = NULL;
            printf("end with no codec buffer\n");
            break;
        }

        if (ptr->current_state== Frame_Buffer_State_Transform) {

            if ((ret = av_hwframe_transfer_data(ptr->tmp_frame_nv12, ptr->codec_buffer, 0)) < 0) {
                fprintf(stderr, "Error transferring the data to system memory\n");
            }

            //Trans fer Nv12 to Yuv
            sws_scale(data->conv_ctx_yuv, ptr->tmp_frame_nv12->data, ptr->tmp_frame_nv12->linesize, 0, data->codec_ctx->height, ptr->tmp_frame_yuv->data, ptr->tmp_frame_yuv->linesize);
            sws_scale(data->conv_ctx, ptr->tmp_frame_yuv->data, ptr->tmp_frame_yuv->linesize, 0, data->codec_ctx->height, ptr->frame_buffer->data, ptr->frame_buffer->linesize);
            ptr->current_state = Frame_Buffer_State_Read;
            ptr = ptr->next_buffer;
        }

    }

}


//PRIIVATE FUNCTION------------------------------
void Decoder::AddVideoBuffer(VideoList** headRef, int idx, VideoData *data) {

    VideoList* newNode = create_Video_buffer(idx,data);
    if (*headRef == NULL) {
        *headRef = newNode;
        VideoIDGenerator = VideoIDGenerator + 1;
        return;
    }
    else {
        VideoList* current = *headRef;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = newNode;
        VideoIDGenerator = VideoIDGenerator + 1;
    }
}

void Decoder::ClearAllVideoBuffer(VideoList** headRef) {

    if (*headRef == NULL) {
        return;
    }

    VideoList* current = *headRef;
    while (current != NULL) {
        VideoList* next = current->next;
        clearVideoData(current->data);
        free(current);
        current = next;
    }
    *headRef = NULL;
}

void Decoder::ClearVideoBuffer(VideoList* Buffer) {

    if (Buffer == nullptr) {
        return;
    }  
        clearVideoData(Buffer->data);
        free(Buffer);
}

void Decoder::clearVideoData(VideoData* data) {
    
    if (data->packet != nullptr) av_free(data->packet);
    if (data->codec_ctx != nullptr) avcodec_close(data->codec_ctx);
   if (data->fmt_ctx != nullptr) avformat_free_context(data->fmt_ctx);
   ReadBuffer* delete_node = data->ReadBufferHead;
   while (delete_node !=nullptr) {
          ReadBuffer* next_node = delete_node->next_buffer;
          av_frame_free(&delete_node->codec_buffer);
          av_frame_free(&delete_node->frame_buffer);
          av_frame_free(&delete_node->tmp_frame_nv12);
          av_frame_free(&delete_node->tmp_frame_yuv);
          free(delete_node->internal_buffer);
          free(delete_node->internal_buffer_yuv);
          free(delete_node);
          delete_node = next_node;
   }

}

bool Decoder::set_decoder(VideoData* data) {

    data->video_stream = data->fmt_ctx->streams[data->stream_idx];
    data->codec_ctxpar = data->video_stream->codecpar;
    data->decoder = avcodec_find_decoder(data->codec_ctxpar->codec_id);

    // CHECK IF SUPPORT¡@¢ÖARDWARE DECODE
    for (int i = 0;; i++) {
        const AVCodecHWConfig* config = avcodec_get_hw_config(data->decoder, i);
        if (!config) {
            fprintf(stderr, "Decoder %s does not support device type %s.\n",
                data->decoder->name, av_hwdevice_get_type_name(data->type));
        }
        if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
            config->device_type == data->type) {
            hw_pix_fmt = config->pix_fmt;
            break;
        }
    }

    //--------------------------------------------------------------------

    data->codec_ctx = avcodec_alloc_context3(data->decoder);
    avcodec_parameters_to_context(data->codec_ctx, data->codec_ctxpar);

    data->codec_ctx->get_format = get_hw_format;
    if (hw_decoder_init(data->codec_ctx, data->type) < 0) {
        printf("hw_decoder_error");
        return false;
    }

    if (data->decoder == NULL) {
        std::cout << "faild to find decorder" << std::endl;
        clearVideoData(data);
        return false;
    }

    return true;

    
}
