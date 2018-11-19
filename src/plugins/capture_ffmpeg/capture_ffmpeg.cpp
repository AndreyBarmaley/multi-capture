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

#include <unistd.h>
#include <cctype>

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
    int			stream_index;

    capture_ffmpeg_t() : is_used(false), is_debug(false), format_ctx(NULL), codec_ctx(NULL), stream_index(-1) {}

    void clear(void)
    {
	is_used = false;
	is_debug = false;
	surface.reset();
	format_ctx = NULL;
	codec_ctx = NULL;
	stream_index = -1;
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
    return 20181119;
}

#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

std::string capture_ffmpeg_v4l2_convert_name(__u8* ptr, int sz)
{
    std::string res;

    for(int it = 0; it < sz; ++it)
    {
	if(! isgraph(ptr[it])) break;
	res.append(1, tolower(ptr[it]));
    }

    return res;
}

int capture_ffmpeg_v4l2_channel_input(const std::string & dev, const std::string & channel)
{
    int fd = open(dev.c_str(), O_RDONLY);
    int res = 0;

    if(0 <= fd)
    {
	for(int index = 0;;index++)
	{
    	    struct v4l2_input st_input;
    	    memset(&st_input, 0, sizeof(st_input));
    	    st_input.index = index;

    	    if(ioctl(fd, VIDIOC_ENUMINPUT, &st_input) < 0)
        	break;

	    std::string input = capture_ffmpeg_v4l2_convert_name(st_input.name, 32);
	    VERBOSE("available input: " << input);

    	    if(0 == channel.compare(input))
	    {
		VERBOSE("selected input: " << channel << ", " << index);
		res = index;
	    }
	}

	close(fd);
    }

    return res;
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
    av_register_all();
    avformat_network_init();
    avdevice_register_all();
#else
    av_register_all();
    avdevice_register_all();
#endif

    AVInputFormat *pFormatInput = av_find_input_format(capture_ffmpeg_format.c_str());

#ifdef FFMPEG_NEW_API
    AVDictionary* v4l2Params = NULL;
    AVDictionary** curParams = NULL;

    if(capture_ffmpeg_format == "video4linux2")
    {
	if(config.hasKey("video4linux2:standard"))
	{
	    std::string v4l2_standard = String::toUpper(config.getString("video4linux2:standard", "PAL"));
	    DEBUG("params: " << "video4linux2:standard = " << v4l2_standard);
	    av_dict_set(& v4l2Params, "video_standard", v4l2_standard.c_str(), 0);
	    curParams = & v4l2Params;
	}

	if(config.hasKey("video4linux2:input"))
	{
	    std::string v4l2_input = String::toLower(config.getString("video4linux2:input", "s-video"));
	    DEBUG("params: " << "video4linux2:input = " << v4l2_input);
	    int input = capture_ffmpeg_v4l2_channel_input(capture_ffmpeg_device, v4l2_input);
	    av_dict_set_int(& v4l2Params, "video_input", input, 0);
	    curParams = & v4l2Params;
	}

	if(config.hasKey("video4linux2:size"))
	{
	    Size v4l2_size = config.getSize("video4linux2:size", Size(720, 576));
	    DEBUG("params: " << "video4linux2:size = " << v4l2_size.toString());
	    std::string strsz = StringFormat("%1x%2").arg(v4l2_size.w).arg(v4l2_size.h);
	    av_dict_set(& v4l2Params, "video_size", strsz.c_str(), 0);
	    curParams = & v4l2Params;
	}
    }

    err = avformat_open_input(& st->format_ctx, capture_ffmpeg_device.c_str(), pFormatInput, curParams);
#else
    AVFormatParameters v4l2Params;
    AVFormatParameters* curParams = NULL;

    if(capture_ffmpeg_format == "video4linux2")
    {
	if(config.hasKey("video4linux2:standard"))
	{
	    std::string v4l2_standard = String::toUpper(config.getString("video4linux2:standard", "PAL"));
	    DEBUG("params: " << "video4linux2:standard = " << v4l2_standard);
	    v4l2Params.standard = v4l2_standard.c_str();
	    curParams = & v4l2Params;
	}

	if(config.hasKey("video4linux2:input"))
	{
	    std::string v4l2_input = String::toLower(config.getString("video4linux2:input", "s-video"));
	    DEBUG("params: " << "video4linux2:input = " << v4l2_input);
	    v4l2Params.channel = capture_ffmpeg_v4l2_channel_input(capture_ffmpeg_device, v4l2_input);
	    curParams = & v4l2Params;
	}

	if(config.hasKey("video4linux2:size"))
	{
	    Size v4l2_size = config.getSize("video4linux2:size", Size(720, 576));
	    DEBUG("params: " << "video4linux2:size = " << v4l2_size.toString());
	    v4l2Params.width = v4l2_size.w;
	    v4l2Params.height = v4l2_size.h;
	    curParams = & v4l2Params;
	}
    }

    err = av_open_input_file(& st->format_ctx, capture_ffmpeg_device.c_str(), pFormatInput, 0, curParams);
#endif
    if(err < 0)
    {
    	ERROR("unable to open device: " << capture_ffmpeg_device << ", error: " << err);
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
    	ERROR("unable to find stream info" << ", error: " << err);
    	return NULL;
    }

    // find video stream
    AVStream* av_stream = NULL;
    int videoStreamIndex = 0;
    for(; videoStreamIndex < st->format_ctx->nb_streams; ++videoStreamIndex)
    {
#ifdef FFMPEG_NEW_API
	AVCodecParameters* codec = st->format_ctx->streams[videoStreamIndex]->codecpar;
#else
	AVCodecContext* codec = st->format_ctx->streams[videoStreamIndex]->codec;
#endif
	if(codec->codec_type == AVMEDIA_TYPE_VIDEO)
	{
	    st->stream_index = videoStreamIndex;
	    av_stream = st->format_ctx->streams[videoStreamIndex];
	    break;
	}
    }

    if(NULL == av_stream)
    {
    	ERROR("unable to find video stream" << ", error: " << err);
    	return NULL;
    }

    // find codec context
#ifdef FFMPEG_NEW_API
    AVCodec* codec = avcodec_find_decoder(av_stream->codecpar->codec_id);
    st->codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(st->codec_ctx, av_stream->codecpar);
    err = avcodec_open2(st->codec_ctx, codec, NULL);
#else
    st->codec_ctx = av_stream->codec;
    AVCodec* codec = avcodec_find_decoder(st->codec_ctx->codec_id);
    err = avcodec_open(st->codec_ctx, codec);
#endif
    if(err < 0)
    {
    	ERROR("unable to open codec" << ", error: " << err);
    	return NULL;
    }

#ifdef FFMPEG_NEW_API
    if(curParams) av_dict_free(curParams);
#endif

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
        if(packet.stream_index == st->stream_index)
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
