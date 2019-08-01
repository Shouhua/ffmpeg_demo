/* save first frame */
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avconfig.h>
#include <libavutil/pixfmt.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>

void save_frame(AVFrame *pFrame, int width, int height, int frame) {
    FILE *pFile;
    char szFilename[32];
    int y;

    //生成文件名称
    sprintf(szFilename, "frame%d.ppm", frame);

    //创建或打开文件
    pFile = fopen(szFilename, "wb");

    if (pFile == NULL) {
        return;
    }

    //写入头信息
    fprintf(pFile, "P6\n%d %d\n225\n", width, height);

    //写入数据
    for (y = 0; y < height; y++)
        fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);
    fclose(pFile);
}

int main(int argc, char *argv[]) {
    if(argc < 2) { fprintf(stderr, "Usage: %s <input file>\n", argv[0]);
        return -1;
    }

    AVFormatContext *fmt_ctx = NULL;

    if(avformat_network_init() != 0) {
        fprintf(stderr, "Failed in avformat_network_init\n");
        return -1;
    }

    if(avformat_open_input(&fmt_ctx, argv[1], NULL, NULL) != 0) {
        fprintf(stderr, "Failed in avformat_open_input\n");
        return -1;
    } 

    if(avformat_find_stream_info(fmt_ctx, NULL) != 0) {
        fprintf(stderr, "Failed in avformat_find_stream_info\n");
        return -1;
    }
    av_dump_format(fmt_ctx, 0, argv[1], 0);

    int stream_index = -1;
    for(unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
        int codec_type = fmt_ctx->streams[i]->codecpar->codec_type;
        /* fprintf(stdout, "codec_type: %d\n", codec_type); */
        if(codec_type == AVMEDIA_TYPE_VIDEO) {
            stream_index = i;
        }
    }
    if(stream_index == -1) {
        fprintf(stderr, "Failed to get video stream\n");
        return -1;
    }

    AVCodecParameters *codec_par = fmt_ctx->streams[stream_index]->codecpar;
    AVCodec *codec = avcodec_find_decoder(codec_par->codec_id);
    /* printf("video codec name: %s\t%s\n", codec->name, codec->long_name); */
    AVCodecContext *codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, codec_par); 
    avcodec_open2(codec_ctx, codec, NULL);

    /* AVRational rational = av_guess_frame_rate(fmt_ctx, fmt_ctx->streams[stream_index], NULL); */
    /* fprintf(stdout, "video framterate=%d,%d\n", rational.num, rational.den); */

    /* int ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0); */
    /* fprintf(stdout, "find_best_stream result: %d\n", ret); */

    AVFrame *frame = NULL;
    frame = av_frame_alloc();
    AVFrame *frame_rgb = NULL;
    frame_rgb = av_frame_alloc();
    AVPacket *packet = NULL;
    packet = av_packet_alloc();
    /* av_init_packet(packet); */
    int num_of_frame = 0;

    unsigned char *buffer = NULL;
    int num_of_byte = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, AV_INPUT_BUFFER_PADDING_SIZE);
    buffer = (unsigned char *)av_malloc(num_of_byte * sizeof(unsigned char));
    av_image_fill_arrays(frame_rgb->data, frame_rgb->linesize, buffer, AV_PIX_FMT_RGB24, codec_ctx->width, codec_ctx->height, AV_INPUT_BUFFER_PADDING_SIZE);

    struct SwsContext *sws_ctx = NULL;
    sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt, codec_ctx->width, codec_ctx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR,
            NULL, NULL, NULL);

    int result = -1;
    while(av_read_frame(fmt_ctx, packet) >= 0) {
        if(packet->stream_index == stream_index) { /* Video stream */
            fprintf(stdout, "packet size: %d\n", packet->size);
            if(packet->size) {
                result = avcodec_send_packet(codec_ctx, packet);
                if(result < 0) {
                    fprintf(stderr, "Error sending a packet for decoding, result=%d\n", result);
                    break;
                }
                while(result >= 0) {
                    result = avcodec_receive_frame(codec_ctx, frame);
                    fprintf(stdout, "avcodec_receive_frame result: %d, AVERROR(EAGAIN): %d, AVERROR_EOF: %d\n", result,AVERROR(EAGAIN), AVERROR_EOF);
                    if(result == AVERROR(EAGAIN) || result == AVERROR_EOF) {
                        break;
                    }
                    fprintf(stdout, "av_get_picture_type_char: %c\n", av_get_picture_type_char(frame->pict_type));
                    fprintf(stdout, "time_base numberator: %d, denominator: %d\n",
                            fmt_ctx->streams[stream_index]->time_base.num,
                            fmt_ctx->streams[stream_index]->time_base.den);
                    fprintf(stdout, "read video No.%d frame, pts=%lld, timestamp=%f seconds\n",
                            codec_ctx->frame_number,
                            frame->pts,
                            frame->pts * av_q2d(fmt_ctx->streams[stream_index]->time_base));
                    ++num_of_frame;
                    sws_scale(sws_ctx, (const uint8_t *const *)frame->data, frame->linesize, 0, codec_ctx->height, frame_rgb->data, frame_rgb->linesize);
                    if(codec_ctx->frame_number == 1) {
                        save_frame(frame_rgb, codec_ctx->width, codec_ctx->height, 1);
                    }
                }
            }
        }
    }
    av_packet_unref(packet);
    av_frame_unref(frame);
    av_frame_unref(frame_rgb);
    av_free(frame);
    av_free(frame_rgb);
    fprintf(stdout, "number of frame: %d\n", num_of_frame);

    if(NULL != codec_ctx) {
        avcodec_free_context(&codec_ctx);
    }
    /* clean_sws_ctx: */
    if(NULL != sws_ctx) {
        sws_freeContext(sws_ctx);
    }
    av_freep(&fmt_ctx->pb->buffer);
    av_freep(&fmt_ctx->pb);
    avformat_close_input(&fmt_ctx);
    return 0;
}
