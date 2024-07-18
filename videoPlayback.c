#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <SDL2/SDL.h>

void play_video(const char *filename) {
    AVFormatContext *format_context = avformat_alloc_context();
    if (!format_context) {
        fprintf(stderr, "Could not allocate format context\n");
        return;
    }

    if (avformat_open_input(&format_context, filename, NULL, NULL) != 0) {
        fprintf(stderr, "Could not open file\n");
        return;
    }

    if (avformat_find_stream_info(format_context, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        return;
    }

    int video_stream_index = -1;
    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    if (video_stream_index == -1) {
        fprintf(stderr, "Could not find video stream\n");
        return;
    }

    AVCodecParameters *codec_parameters = format_context->streams[video_stream_index]->codecpar;
    AVCodec *codec = avcodec_find_decoder(codec_parameters->codec_id);
    if (!codec) {
        fprintf(stderr, "Could not find codec\n");
        return;
    }

    AVCodecContext *codec_context = avcodec_alloc_context3(codec);
    if (!codec_context) {
        fprintf(stderr, "Could not allocate codec context\n");
        return;
    }

    if (avcodec_parameters_to_context(codec_context, codec_parameters) < 0) {
        fprintf(stderr, "Could not copy codec parameters\n");
        return;
    }

    if (avcodec_open2(codec_context, codec, NULL) < 0) {
        fprintf(stderr, "Could not open codec\n");
        return;
    }

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        return;
    }

    SDL_Window *window = SDL_CreateWindow("Video Player",
                                          SDL_WINDOWPOS_UNDEFINED,
                                          SDL_WINDOWPOS_UNDEFINED,
                                          codec_context->width,
                                          codec_context->height,
                                          0);

    if (!window) {
        fprintf(stderr, "Could not create window - %s\n", SDL_GetError());
        return;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);
    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_YV12,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             codec_context->width,
                                             codec_context->height);

    struct SwsContext *sws_context = sws_getContext(codec_context->width,
                                                    codec_context->height,
                                                    codec_context->pix_fmt,
                                                    codec_context->width,
                                                    codec_context->height,
                                                    AV_PIX_FMT_YUV420P,
                                                    SWS_BILINEAR,
                                                    NULL,
                                                    NULL,
                                                    NULL);

    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    AVFrame *yuv_frame = av_frame_alloc();
    int yuv_frame_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, codec_context->width, codec_context->height, 1);
    uint8_t *yuv_frame_buffer = (uint8_t *)av_malloc(yuv_frame_size);
    av_image_fill_arrays(yuv_frame->data, yuv_frame->linesize, yuv_frame_buffer, AV_PIX_FMT_YUV420P, codec_context->width, codec_context->height, 1);

    int ret;
    int quit = 0;
    SDL_Event event;
    while (av_read_frame(format_context, packet) >= 0 && !quit) {
        if (packet->stream_index == video_stream_index) {
            ret = avcodec_send_packet(codec_context, packet);
            if (ret < 0) {
                fprintf(stderr, "Error sending packet to codec\n");
                break;
            }

            ret = avcodec_receive_frame(codec_context, frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                av_packet_unref(packet);
                continue;
            } else if (ret < 0) {
                fprintf(stderr, "Error receiving frame from codec\n");
                break;
            }

            sws_scale(sws_context,
                      (const uint8_t *const *)frame->data,
                      frame->linesize,
                      0,
                      codec_context->height,
                      yuv_frame->data,
                      yuv_frame->linesize);

            SDL_UpdateYUVTexture(texture,
                                 NULL,
                                 yuv_frame->data[0],
                                 yuv_frame->linesize[0],
                                 yuv_frame->data[1],
                                 yuv_frame->linesize[1],
                                 yuv_frame->data[2],
                                 yuv_frame->linesize[2]);

            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
        }

        av_packet_unref(packet);

        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    quit = 1;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_RIGHT) {
                        int64_t seek_target = format_context->streams[video_stream_index]->duration / 10;
                        av_seek_frame(format_context, video_stream_index, seek_target, AVSEEK_FLAG_ANY);
                    } else if (event.key.keysym.sym == SDLK_LEFT) {
                        int64_t seek_target = -format_context->streams[video_stream_index]->duration / 10;
                        av_seek_frame(format_context, video_stream_index, seek_target, AVSEEK_FLAG_ANY);
                    }
                    break;
            }
        }
    }

    av_packet_free(&packet);
    av_frame_free(&frame);
    av_frame_free(&yuv_frame);
    av_free(yuv_frame_buffer);
    sws_freeContext(sws_context);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    avcodec_free_context(&codec_context);
    avformat_close_input(&format_context);
    avformat_free_context(format_context);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <video_file>\n", argv[0]);
        return -1;
    }

    // av_register_all();  // This is deprecated and can be removed in newer FFmpeg versions.
    play_video(argv[1]);

    return 0;
}

