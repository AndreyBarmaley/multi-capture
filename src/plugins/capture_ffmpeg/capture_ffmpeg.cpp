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

#include "../../settings.h"

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

// centos6, ffmpeg 2.6.8:  LIBAVFORMAT_VERSION_INT == AV_VERSION_INT(56,25,101)
// centos7, ffmpeg 2.8.15:  LIBAVFORMAT_VERSION_INT == AV_VERSION_INT(56,40,101)
// centos7, ffmpeg 4.1.4:  LIBAVFORMAT_VERSION_INT == AV_VERSION_INT(58,20,100)
// fedora24, ffmpeg 3.1.9: LIBAVFORMAT_VERSION_INT == AV_VERSION_INT(57,41,100)
// fedora26, ffmpeg 3.3.7: LIBAVFORMAT_VERSION_INT == AV_VERSION_INT(57,71,100)

#if LIBAVFORMAT_VERSION_MAJOR < 53
#define FFMPEG_OLD_API 1
#else
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
#define CAPTURE_FFMPEG_SPOOL 16
#endif

capture_ffmpeg_t capture_ffmpeg_vals[CAPTURE_FFMPEG_SPOOL];

const char* capture_ffmpeg_get_name(void)
{
    return "capture_ffmpeg";
}

int capture_ffmpeg_get_version(void)
{
    return 20210130;
}

#if defined(__WIN32__)
int capture_ffmpeg_v4l2_channel_input(const std::string & dev, const std::string & channel)
{
    return 0;
}

#else
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
#endif

char* string_dup(const std::string & str)
{   
    size_t len = str.size();
    char*  res = (char*) malloc(len + 1);
    std::copy(str.begin(), str.end(), res);
    res[len] = 0;
    return res;
}

#ifndef FFMPEG_OLD_API
void ffmpeg_list_devices(void)
{
    // list format
    AVInputFormat* format = NULL;
    while(NULL != (format = av_input_video_device_next(format)))
    {
        DEBUG("available format: " << format->name << ", (" << format->long_name << ")");
    
        AVDeviceInfoList* device_list = NULL;

        if(format->get_device_list &&
            0 == avdevice_list_input_sources(format, NULL, NULL, &device_list))
        {
            for(int ii = 0; ii < device_list->nb_devices; ++ii) if(device_list->devices[ii])
            {
                DEBUG("available source: " << device_list->devices[ii]->device_name << ", (" << device_list->devices[ii]->device_description << ")");
            }
        }
        else
        {
            DEBUG("sources: empty");
        }
    
        avdevice_free_list_devices(&device_list);
    }
}
#endif

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

    st->is_used = true;
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

#ifdef FF_OLD_API
    avdevice_register_all();
#else
#if LIBAVFORMAT_VERSION_MAJOR < 58
    av_register_all();
#endif
    avformat_network_init();
    avdevice_register_all();

    ffmpeg_list_devices();
#endif

    AVInputFormat *pFormatInput = av_find_input_format(capture_ffmpeg_format.c_str());

#ifdef FFMPEG_OLD_API
    char* v4l2_standard_dup = NULL;
#else
    AVDictionary** curParams = NULL;
#endif

    auto localFree = [=](){
#ifdef FFMPEG_OLD_API
        if(v4l2_standard_dup) free(v4l2_standard_dup);
#else
        if(curParams) av_dict_free(curParams);
#endif
    };

#ifdef FFMPEG_OLD_API
    AVFormatParameters v4l2Params;
    AVFormatParameters* curParams = NULL;

    if(capture_ffmpeg_format == "video4linux2")
    {
	if(config.hasKey("video4linux2:standard"))
	{
	    std::string v4l2_standard = String::toUpper(config.getString("video4linux2:standard", "PAL"));
	    DEBUG("params: " << "video4linux2:standard = " << v4l2_standard);
	    v4l2_standard_dup = string_dup(v4l2_standard);
	    v4l2Params.standard = v4l2_standard_dup;
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
	    Size v4l2_size = JsonUnpack::size(config, "video4linux2:size", Size(720, 576));
	    DEBUG("params: " << "video4linux2:size = " << v4l2_size.toString());
	    v4l2Params.width = v4l2_size.w;
	    v4l2Params.height = v4l2_size.h;
	    curParams = & v4l2Params;
	}
    }

    err = av_open_input_file(& st->format_ctx, capture_ffmpeg_device.c_str(), pFormatInput, 0, curParams);
#else
    AVDictionary* v4l2Params = NULL;

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
	    Size v4l2_size = JsonUnpack::size(config, "video4linux2:size", Size(720, 576));
	    DEBUG("params: " << "video4linux2:size = " << v4l2_size.toString());
	    std::string strsz = StringFormat("%1x%2").arg(v4l2_size.w).arg(v4l2_size.h);
	    av_dict_set(& v4l2Params, "video_size", strsz.c_str(), 0);
	    curParams = & v4l2Params;
	}

    }

    if(config.hasKey("init:timeout"))
    {
	int timeout = config.getInteger("init:timeout", 0);
	if(0 < timeout)
	{
	    std::string snum = String::number(timeout);
	    DEBUG("params: " << "init:timeout = " << snum);
	    av_dict_set(& v4l2Params, "timeout", snum.c_str(), 0);
	    curParams = & v4l2Params;
	}
    }

    err = avformat_open_input(& st->format_ctx, capture_ffmpeg_device.c_str(), pFormatInput, curParams);
#endif
    if(err < 0)
    {
    	ERROR("unable to open device: " << capture_ffmpeg_device << ", error: " << err);
	st->is_used = false;
        localFree();
    	return NULL;
    }

    DEBUG("open input: " << capture_ffmpeg_device << " success");

    // retrieve stream information
#ifdef FFMPEG_OLD_API
    err = av_find_stream_info(st->format_ctx);
#else
    err = avformat_find_stream_info(st->format_ctx, NULL);
#endif
    if(err < 0)
    {
    	ERROR("unable to find stream info" << ", error: " << err);
	st->is_used = false;
        localFree();
    	return NULL;
    }

    // find video stream
    AVStream* av_stream = NULL;
    int videoStreamIndex = 0;
    for(; videoStreamIndex < st->format_ctx->nb_streams; ++videoStreamIndex)
    {
#if LIBAVFORMAT_VERSION_MAJOR > 56
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
	st->is_used = false;
        localFree();
    	return NULL;
    }

    // find codec context
#if LIBAVFORMAT_VERSION_MAJOR > 56
    AVCodec* codec = avcodec_find_decoder(av_stream->codecpar->codec_id);
    st->codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(st->codec_ctx, av_stream->codecpar);
    err = avcodec_open2(st->codec_ctx, codec, NULL);
#else
    st->codec_ctx = av_stream->codec;
    AVCodec* codec = avcodec_find_decoder(st->codec_ctx->codec_id);
    err = avcodec_open2(st->codec_ctx, codec, NULL);
#endif
    if(err < 0)
    {
    	ERROR("unable to open codec" << ", error: " << err);
	st->is_used = false;
        localFree();
    	return NULL;
    }

    localFree();

    DEBUG("index " << devindex << " started...");
    return st;
}

void capture_ffmpeg_quit(void* ptr)
{
    capture_ffmpeg_t* st = static_cast<capture_ffmpeg_t*>(ptr);
    if(st->is_debug) VERBOSE("version: " << capture_ffmpeg_get_version());

#if LIBAVFORMAT_VERSION_MAJOR > 56
    if(st->codec_ctx) avcodec_free_context(& st->codec_ctx);
#endif
#ifdef FFMPEG_OLD_API
    if(st->codec_ctx) avcodec_close(st->codec_ctx);
    if(st->format_ctx) av_close_input_file(st->ormat_cts);
#else
    if(st->codec_ctx) avcodec_close(st->codec_ctx);
    if(st->format_ctx) avformat_close_input(& st->format_ctx);
#endif

    st->clear();
}

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
#define Rmask 0xff000000
#define Gmask 0x00ff0000
#define Bmask 0x0000ff00
#define Amask 0x000000ff
#else
#define Rmask 0x000000ff
#define Gmask 0x0000ff00
#define Bmask 0x00ff0000
#define Amask 0xff000000
#endif

#if LIBAVFORMAT_VERSION_MAJOR > 56
int avcodec_decode_video22(AVCodecContext* avctx, AVFrame* frame, int* got_frame, AVPacket* pkt)
    {
    int ret;
    *got_frame = 0;

    if (pkt)
    {
        ret = avcodec_send_packet(avctx, pkt);
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
#else
int avcodec_decode_video22(AVCodecContext* avctx, AVFrame* frame, int* got_frame, AVPacket* pkt)
{
    return avcodec_decode_video2(avctx, frame, got_frame, pkt);
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

#ifdef FFMPEG_OLD_API
    AVFrame* pFrame = avcodec_alloc_frame();
    AVFrame* pFrameRGB = avcodec_alloc_frame();

    if(pFrameRGB == NULL)
    {
	ERROR("pFrameRGB is NULL");
	return -1;
    }

    int numBytes = avpicture_get_size(PIX_FMT_RGB24, st->codec_ctx->width, st->codec_ctx->height);
    uint8_t* buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

    struct SwsContext* sws_ctx = sws_getContext(st->codec_ctx->width, st->codec_ctx->height, st->codec_ctx->pix_fmt,
                st->codec_ctx->width, st->codec_ctx->height, PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);

    avpicture_fill((AVPicture *) pFrameRGB, buffer, PIX_FMT_RGB24, st->codec_ctx->width, st->codec_ctx->height);
#else
    AVFrame* pFrame = av_frame_alloc();
    AVFrame* pFrameRGB = av_frame_alloc();

    if(pFrameRGB == NULL)
    {
	ERROR("pFrameRGB is NULL");
	return -1;
    }

    int numBytes = av_image_get_buffer_size (AV_PIX_FMT_RGB24, st->codec_ctx->width, st->codec_ctx->height, 1);
    uint8_t* buffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));

    struct SwsContext* sws_ctx = sws_getContext(st->codec_ctx->width, st->codec_ctx->height, st->codec_ctx->pix_fmt,
                st->codec_ctx->width, st->codec_ctx->height, AV_PIX_FMT_RGB24, SWS_BILINEAR, NULL, NULL, NULL);

    av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, buffer, AV_PIX_FMT_RGB24, st->codec_ctx->width, st->codec_ctx->height, 1);
#endif

    AVPacket packet;
    av_init_packet(&packet);

    int frameFinished = 0;

    while(av_read_frame(st->format_ctx, &packet) >= 0)
    {
        if(packet.stream_index == st->stream_index)
        {
            if(0 > avcodec_decode_video22(st->codec_ctx, pFrame, &frameFinished, &packet))
	    {
		break;
	    }

            if(frameFinished)
            {
                sws_scale(sws_ctx, (uint8_t const* const*) pFrame->data, pFrame->linesize, 0,
                            st->codec_ctx->height, pFrameRGB->data, pFrameRGB->linesize);

#if SDL_VERSION_ATLEAST(2,0,5)
                SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(pFrameRGB->data[0],
                            st->codec_ctx->width, st->codec_ctx->height,
                            24, pFrameRGB->linesize[0], SDL_PIXELFORMAT_RGB24);
#else
                SDL_Surface* sf = SDL_CreateRGBSurfaceFrom(pFrameRGB->data[0],
                            st->codec_ctx->width, st->codec_ctx->height,
                            24, pFrameRGB->linesize[0], Rmask, Gmask, Bmask, Amask);
#endif
                av_frame_unref(pFrame);
                av_frame_unref(pFrameRGB);
 
		st->surface = Surface::copy(sf);
                break;
            }
        }

#ifdef FFMPEG_OLD_API
        av_free_packet(&packet);
#else
    	av_packet_unref(&packet);
#endif
    }

#ifdef FFMPEG_OLD_API
    av_free_packet(&packet);
#else
    av_packet_unref(&packet);
#endif

    av_free(buffer);
    sws_freeContext(sws_ctx);

    av_free(pFrameRGB);
    av_free(pFrame);

    return frameFinished ? 0 : -1;
}

const Surface & capture_ffmpeg_get_surface(void* ptr)
{
    capture_ffmpeg_t* st = static_cast<capture_ffmpeg_t*>(ptr);
    return st->surface;
}

#ifdef __cplusplus
}
#endif
