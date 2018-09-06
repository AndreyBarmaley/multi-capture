/***************************************************************************
 *   Copyright (C) 2018 by FlyCapture team <public.irkutsk@gmail.com>      *
 *                                                                         *
 *   Part of the FlyCapture engine:                                        *
 *   https://github.com/AndreyBarmaley/fly-capture                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "libavdevice/avdevice.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libswscale/swscale.h"

#if LIBAVFORMAT_VERSION_MAJOR >= 53
#define FFMPEG_NEW_API 1
#endif

#ifdef FFMPEG_NEW_API
#include "libavutil/imgutils.h"
#endif

struct capture_ffmpeg_t
{
    bool		is_used;
    bool		is_debug;
    Surface		surface;

    AVFormatContext*	format_ctx;
    AVCodecContext*	codec_ctx;
    size_t		stream_index;

    capture_ffmpeg_t() : is_used(false), is_debug(false), format_ctx(NULL), codec_ctx(NULL), stream_index(0) {}

    void clear(void)
    {
	is_used = false;
	is_debug = false;
	surface.reset();
	format_ctx = NULL;
	codec_ctx = NULL;
	stream_index = 0;
    }
};

#ifndef CAPTURE_FFMPEG_SPOOL
#define CAPTURE_FFMPEG_SPOOL 4
#endif

capture_ffmpeg_t capture_ffmpeg_vals[CAPTURE_FFMPEG_SPOOL];

const char* capture_ffmpeg_get_name(void)
{
    return "capture_ffmpeg";
}

int capture_ffmpeg_get_version(void)
{
    return 20180817;
}

void* capture_ffmpeg_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_ffmpeg_get_version());

    int devindex = 0;
    for(; devindex < CAPTURE_FFMPEG_SPOOL; ++devindex)
	if(! capture_ffmpeg_vals[devindex].is_used) break;

    if(CAPTURE_FFMPEG_SPOOL <= devindex)
    {
	ERROR("spool is busy, max limit: " << CAPTURE_FFMPEG_SPOOL);
	return NULL;
    }

    DEBUG("spool index: " << devindex);
    capture_ffmpeg_t* st = & capture_ffmpeg_vals[devindex];

    st->is_debug = config.getBoolean("debug", false);
    std::string capture_ffmpeg_device = config.getString("device");
    std::string capture_ffmpeg_format = config.getString("format");

    DEBUG("params: " << "device = " << capture_ffmpeg_device);
    DEBUG("params: " << "format = " << capture_ffmpeg_format);

    if(st->is_debug)
    {
	av_log_set_level(AV_LOG_DEBUG);
    }
    else
    {
	av_log_set_level(AV_LOG_ERROR);
    }

    int err = 0;

#ifdef FFMPEG_NEW_API
#if LIBAVFORMAT_VERSION_MAJOR == 56
    av_register_all();
#endif
    avformat_network_init();
    avdevice_register_all();
#else
    av_register_all();
    avdevice_register_all();
#endif

    AVInputFormat *pFormatInput = av_find_input_format(capture_ffmpeg_format.c_str());

#ifdef FFMPEG_NEW_API
    err = avformat_open_input(& st->format_ctx, capture_ffmpeg_device.c_str(), pFormatInput, NULL);
#else
    err = av_open_input_file(& st->format_ctx, capture_ffmpeg_device.c_str(), pFormatInput, 0, NULL);
#endif
    if(err < 0)
    {
    	ERROR("unable to open device: " << capture_ffmpeg_device);
    	return NULL;
    }

    // retrieve stream information
#ifdef FFMPEG_NEW_API
    err = avformat_find_stream_info(st->format_ctx, NULL);
#else
    err = av_find_stream_info(st->format_ctx);
#endif
    if(err < 0)
    {
    	ERROR("unable to find stream info");
    	return NULL;
    }

    // find video stream
    st->stream_index = 0;
    while(st->stream_index < st->format_ctx->nb_streams)
    {
#ifdef FFMPEG_NEW_API
	if(st->format_ctx->
	    streams[st->stream_index]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
    		break;
#else
	if(st->format_ctx->
	    streams[st->stream_index]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        	break;
#endif
	st->stream_index++;
    }

    if(st->stream_index == st->format_ctx->nb_streams)
    {
    	ERROR("unable to find video stream");
    	return NULL;
    }

    // find codec context
#ifdef FFMPEG_NEW_API
    AVCodec* codec = avcodec_find_decoder(st->format_ctx->streams[st->stream_index]->codecpar->codec_id);
    st->codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(st->codec_ctx, st->format_ctx->streams[st->stream_index]->codecpar);
    err = avcodec_open2(st->codec_ctx, codec, NULL);
#else
    st->codec_ctx = st->format_ctx->streams[st->stream_index]->codec;
    AVCodec* codec = avcodec_find_decoder(st->codec_ctx->codec_id);
    err = avcodec_open(st->codec_ctx, codec);
#endif
    if(err < 0)
    {
    	ERROR("unable to open codec");
    	return NULL;
    }

    st->is_used = true;
    return st;
}

void capture_ffmpeg_quit(void* ptr)
{
    capture_ffmpeg_t* st = static_cast<capture_ffmpeg_t*>(ptr);
    if(st->is_debug) VERBOSE("version: " << capture_ffmpeg_get_version());

#ifdef FFMPEG_NEW_API
    // close the codec
    avcodec_free_context(& st->codec_ctx);
    avformat_close_input(& st->format_ctx);
#else
    if(st->codec_ctx) avcodec_close(st->codec_ctx);
    // Close the video file
    if(st->format_ctx) av_close_input_file(st->format_ctx);
#endif

    st->clear();
}

#ifdef FFMPEG_NEW_API
int avcodec_decode_video_replace(AVCodecContext* avctx, AVFrame* frame, int* got_frame, AVPacket* pkt)
{
    int ret;

    *got_frame = 0;

    if (pkt)
    {
        ret = avcodec_send_packet(avctx, pkt);
        // In particular, we don't expect AVERROR(EAGAIN), because we read all
        // decoded frames with avcodec_receive_frame() until done.
        if (ret < 0)
            return ret == AVERROR_EOF ? 0 : ret;
    }

    ret = avcodec_receive_frame(avctx, frame);
    if (ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
        return ret;

    if (ret >= 0)
        *got_frame = 1;

    return 0;
}
#endif

int capture_ffmpeg_frame_action(void* ptr)
{
    capture_ffmpeg_t* st = static_cast<capture_ffmpeg_t*>(ptr);

    if(st->is_debug) DEBUG("version: " << capture_ffmpeg_get_version());

    if(! st->format_ctx || ! st->codec_ctx)
    {
	ERROR("params empty");
	return -1;
    }

#ifdef FFMPEG_NEW_API
    // Allocate video frame
    AVFrame* pFrame = av_frame_alloc();

    // Allocate an AVFrame structure
    AVFrame* pFrameRGB = av_frame_alloc();

    if(pFrameRGB == NULL)
    {
	ERROR("pFrameRGB is NULL");
	return -1;
    }

    // Determine required buffer size and allocate buffer
    int numBytes = av_image_get_buffer_size (AV_PIX_FMT_RGB24, st->codec_ctx->width, st->codec_ctx->height, 1);
    uint8_t* buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

    struct SwsContext* sws_ctx = sws_getContext(st->codec_ctx->width, st->codec_ctx->height, st->codec_ctx->pix_fmt,
                st->codec_ctx->width, st->codec_ctx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
    // of AVPicture
    av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, st->codec_ctx->width, st->codec_ctx->height, 1);
#else
    // Allocate video frame
    AVFrame* pFrame = avcodec_alloc_frame();

    // Allocate an AVFrame structure
    AVFrame* pFrameRGB = avcodec_alloc_frame();

    if(pFrameRGB == NULL)
    {
	ERROR("pFrameRGB is NULL");
	return -1;
    }

    // Determine required buffer size and allocate buffer
    int numBytes = avpicture_get_size(PIX_FMT_RGB24, st->codec_ctx->width, st->codec_ctx->height);

    uint8_t* buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

    struct SwsContext* sws_ctx = sws_getContext(st->codec_ctx->width, st->codec_ctx->height, st->codec_ctx->pix_fmt,
                st->codec_ctx->width, st->codec_ctx->height, PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);

    // Assign appropriate parts of buffer to image planes in pFrameRGB
    // Note that pFrameRGB is an AVFrame, but AVFrame is a superset
    // of AVPicture
    avpicture_fill((AVPicture *) pFrameRGB, buffer, PIX_FMT_RGB24, st->codec_ctx->width, st->codec_ctx->height);
#endif

    // Read frames and save first five frames to disk
    AVPacket packet;
    int frameFinished = 0;

    while(av_read_frame(st->format_ctx, &packet) >= 0)
    {
        // Is this a packet from the video stream?
        if(packet.stream_index == static_cast<int>(st->stream_index))
        {
#ifdef FFMPEG_NEW_API
            // Decode video frame
	    if(0 > avcodec_decode_video_replace(st->codec_ctx, pFrame, &frameFinished, &packet))
	    {
		break;
	    }
#else
            // Decode video frame
            if(0 > avcodec_decode_video2(st->codec_ctx, pFrame, &frameFinished, &packet))
	    {
		break;
	    }
#endif

            // Did we get a video frame?
            if(frameFinished)
            {
                // Convert the image from its native format to RGB
                sws_scale(sws_ctx, (uint8_t const* const*) pFrame->data, pFrame->linesize, 0,
                            st->codec_ctx->height, pFrameRGB->data, pFrameRGB->linesize);

		SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(pFrameRGB->data[0],
			    st->codec_ctx->width, st->codec_ctx->height,
			    24, pFrameRGB->linesize[0], SDL_PIXELFORMAT_RGB24);

		st->surface = Surface(sf).copy();
                break;
            }
        }

#ifdef FFMPEG_NEW_API
        // Free the packet that was allocated by av_read_frame
    	av_packet_unref(&packet);
#else
        // Free the packet that was allocated by av_read_frame
        av_free_packet(&packet);
#endif
    }

#ifdef FFMPEG_NEW_API
    // Free the packet that was allocated by av_read_frame
    av_packet_unref(&packet);
#else
    // Free the packet that was allocated by av_read_frame
    av_free_packet(&packet);
#endif

    av_free(buffer);
    sws_freeContext(sws_ctx);

    av_free(pFrameRGB);
    av_free(pFrame);

    return 0;
}

const Surface & capture_ffmpeg_get_surface(void* ptr)
{
    capture_ffmpeg_t* st = static_cast<capture_ffmpeg_t*>(ptr);
    return st->surface;
}

#ifdef __cplusplus
}
#endif
