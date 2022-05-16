/***************************************************************************
 *   Copyright (C) 2018 by MultiCapture team <public.irkutsk@gmail.com>    *
 *                                                                         *
 *   Part of the MultiCapture engine:                                      *
 *   https://github.com/AndreyBarmaley/multi-capture                       *
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

#include <atomic>
#include <thread>
#include <chrono>
#include <cctype>

#include "../../settings.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace std::chrono_literals;
const int capture_ffmpeg_version = 20220510;

#include "libavdevice/avdevice.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"

// centos6, ffmpeg 2.6.8:  LIBAVFORMAT_VERSION_INT == AV_VERSION_INT(56,25,101)
// centos7, ffmpeg 2.8.15:  LIBAVFORMAT_VERSION_INT == AV_VERSION_INT(56,40,101)
// centos7, ffmpeg 4.1.4:  LIBAVFORMAT_VERSION_INT == AV_VERSION_INT(58,20,100)
// fedora24, ffmpeg 3.1.9: LIBAVFORMAT_VERSION_INT == AV_VERSION_INT(57,41,100)
// fedora26, ffmpeg 3.3.7: LIBAVFORMAT_VERSION_INT == AV_VERSION_INT(57,71,100)
// fedora28, ffmpeg 4.0.4: LIBAVFORMAT_VERSION_INT == AV_VERSION_INT(58,3,100)
// fedora32, ffmpeg 4.2.4: LIBAVFORMAT_VERSION_INT == AV_VERSION_INT(58,29,100)

#if defined(__LINUX__)
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#endif

struct AVFrameDeleter
{
    void operator()(AVFrame* ptr)
    {
        av_frame_free(& ptr);
    }
};

struct AVPacketDeleter
{
    void operator()(AVPacket* ptr)
    {
#if LIBAVFORMAT_VERSION_MAJOR > 56
        av_packet_free(& ptr);
#else
	av_packet_unref(ptr);
#endif
    }
};

struct SwsContextDeleter
{   
    void operator()(SwsContext* ctx)
    {
        sws_freeContext(ctx);
    }
};

class VideoCodec
{
    AVCodecContext* ctxCodec;

public:
    VideoCodec() : ctxCodec(nullptr)
    {
    }

    bool init(const AVStream & stream)
    {
	// find codec context
#if LIBAVFORMAT_VERSION_MAJOR > 56
	AVCodec* codec = avcodec_find_decoder(stream.codecpar->codec_id);
	ctxCodec = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(ctxCodec, stream.codecpar);
	int err = avcodec_open2(ctxCodec, codec, nullptr);
#else
	ctxCodec = stream.codec;
        AVCodec* codec = avcodec_find_decoder(ctxCodec->codec_id);
	int err = avcodec_open2(ctxCodec, codec, nullptr);
#endif
	if(err < 0)
        {
            ERROR("unable to open codec, error: " << err);
            return false;
        }

        return true;
    }

    ~VideoCodec()
    {
#if LIBAVFORMAT_VERSION_MAJOR > 56
        if(ctxCodec)
            avcodec_free_context(&ctxCodec);
#endif
        if(ctxCodec) avcodec_close(ctxCodec);
    }

    bool valid(void) const
    {
        return ctxCodec;
    }

    int width(void) const
    {
        return ctxCodec ? ctxCodec->width : 0;
    }

    int height(void) const
    {
        return ctxCodec ? ctxCodec->height : 0;
    }

    enum AVPixelFormat pixelFormat(void) const
    {
        return ctxCodec ? ctxCodec->pix_fmt : AV_PIX_FMT_NONE;
    }

#if LIBAVFORMAT_VERSION_MAJOR > 56
    int decodeVideo22(AVFrame* frame, int* got_frame, AVPacket* pkt) const
    {
	int ret;
	*got_frame = 0;

	if(pkt)
	{
    	    ret = avcodec_send_packet(ctxCodec, pkt);
    	    if(ret < 0)
        	return ret == AVERROR_EOF ? 0 : ret;
	}

	ret = avcodec_receive_frame(ctxCodec, frame);
	if(ret < 0 && ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
    	    return ret;

	if(ret >= 0)
    	    *got_frame = 1;

	return 0;
    }
#else
    int decodeVideo22(AVFrame* frame, int* got_frame, AVPacket* pkt) const
    {
	return avcodec_decode_video2(ctxCodec, frame, got_frame, pkt);
    }
#endif
};

class VideoFormat
{
    AVFormatContext*	ctxFormat;
    AVDictionary*       v4l2Params;

    std::unique_ptr<uint8_t, decltype(av_free)*> avBuffer{nullptr, av_free};
    std::unique_ptr<SwsContext, SwsContextDeleter> ctxSws;

public:
    VideoFormat() : ctxFormat(nullptr), v4l2Params(nullptr)
    {
    }

    ~VideoFormat()
    {
        if(ctxFormat)
            avformat_close_input(& ctxFormat);
    	if(v4l2Params)
            av_dict_free(&v4l2Params);
    }

    static int v4l2ChannelInput(const std::string & dev, const std::string & channel)
    {
#if defined(__WIN32__)
	return 0;
#else
        auto v4l2ConvertName = [](__u8* ptr, int sz)
        {
            std::string res;

            for(int it = 0; it < sz; ++it)
            {
	        if(! isgraph(ptr[it])) break;
	        res.append(1, tolower(ptr[it]));
            }

            return res;
        };

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

		std::string input = v4l2ConvertName(st_input.name, 32);
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
#endif
    }

    bool init(const std::string & ffmpegFormat, const std::string & ffmpegDevice, const JsonObject & config)
    {
	AVInputFormat* pFormatInput = nullptr;
        if(! ffmpegFormat.empty())
        {
	    pFormatInput = av_find_input_format(ffmpegFormat.c_str());
            if(! pFormatInput)
                ERROR("unknown ffmpeg format: " << ffmpegFormat);
        }

	AVDictionary** curParams = nullptr;

        // example { "video_standard": "PAL", "video_input": "s-video", "video_size": "640x480" }
        if(auto ffmpegOptions = config.getObject("ffmpeg:options"))
        {
            for(auto & key : ffmpegOptions->keys())
            {
                if(JsonType::Integer == ffmpegOptions->getType(key))
                {
                    int err = av_dict_set_int(& v4l2Params, key.c_str(), ffmpegOptions->getInteger(key), 0);
                    if(err < 0)
                        ERROR("av_dict_set_int failed, error: " << err);
                }
                else
                {
                    auto val = ffmpegOptions->getString(key);
                    int err = av_dict_set(& v4l2Params, key.c_str(), val.c_str(), 0);
                    if(err < 0)
                        ERROR("av_dict_set failed, error: " << err);
                }
            }

	    curParams = & v4l2Params;
        }

        // compatible
	if(config.hasKey("init:timeout"))
	{
	    int timeout = config.getInteger("init:timeout", 0);
	    if(0 < timeout)
	    {
		DEBUG("params: " << "init:timeout = " << timeout);
                if(ffmpegDevice.size() > 4)
                {
                    auto proto = ffmpegDevice.substr(0, 4);
                    if(proto == "http")
                    {
                        // timeout microsec
		        av_dict_set_int(& v4l2Params, "timeout", timeout * 1000, 0);
                    }
                    else
                    if(proto == "rtsp")
                    {
                        // socket timeout microsec
                        av_dict_set_int(& v4l2Params, "stimeout", timeout * 1000, 0);
                    }

		    curParams = & v4l2Params;
                }
	    }
	}

	int err = avformat_open_input(& ctxFormat, ffmpegDevice.c_str(), pFormatInput, curParams);
    	if(err < 0)
        {
            ERROR("unable to open input, device: " << ffmpegDevice << ", format: " << ffmpegFormat << ", error: " << err);
            return false;
        }

        return true;
    }

    bool valid(void) const
    {
        return ctxFormat;
    }

    std::pair<AVStream*, int> findVideoStreamIndex(void) const
    {
        int err = avformat_find_stream_info(ctxFormat, nullptr);
        if(err < 0)
        {
            ERROR("unable to find stream info, error: " << err);
            return std::make_pair(nullptr, 0);
        }

        for(int videoStreamIndex = 0; videoStreamIndex < ctxFormat->nb_streams; ++videoStreamIndex)
        {
#if LIBAVFORMAT_VERSION_MAJOR > 56
            AVCodecParameters* codec = ctxFormat->streams[videoStreamIndex]->codecpar;
#else
            AVCodecContext* codec = ctxFormat->streams[videoStreamIndex]->codec;
#endif
            if(codec->codec_type == AVMEDIA_TYPE_VIDEO)
                return std::make_pair(ctxFormat->streams[videoStreamIndex], videoStreamIndex);
        }

        return std::make_pair(nullptr, 0);
    }

    bool frameToSurface(const VideoCodec & videoCodec, int streamIndex, int debug, Surface & result)
    {
        std::unique_ptr<AVFrame, AVFrameDeleter> pFrame(av_frame_alloc());
        std::unique_ptr<AVFrame, AVFrameDeleter> pFrameRGB(av_frame_alloc());
        const AVPixelFormat avPixFmt = AV_PIX_FMT_0RGB;

        if(! avBuffer)
        {
            int numBytes = av_image_get_buffer_size(avPixFmt, videoCodec.width(), videoCodec.height(), 1);
            uint8_t* ptr = (uint8_t*) av_malloc(numBytes);
            avBuffer.reset(ptr);
        }

        if(! ctxSws)
        {
            ctxSws.reset(sws_getContext(videoCodec.width(), videoCodec.height(), videoCodec.pixelFormat(),
                        videoCodec.width(), videoCodec.height(), avPixFmt, SWS_BILINEAR, nullptr, nullptr, nullptr));
        }

        av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, avBuffer.get(), avPixFmt, videoCodec.width(), videoCodec.height(), 1);

        int frameFinished = 0;
        int fixme = 0;

        while(true)
        {
#if LIBAVFORMAT_VERSION_MAJOR > 56
            std::unique_ptr<AVPacket, AVPacketDeleter> pkt(av_packet_alloc());
#else
	    AVPacket _pkt;
	    av_init_packet(& _pkt);
            std::unique_ptr<AVPacket, AVPacketDeleter> pkt(& _pkt);
#endif
            int err = av_read_frame(ctxFormat, pkt.get());
            if(0 != err)
            {
                ERROR("read frame failed, error: " << err);
                break;
            }

            if(pkt->stream_index == streamIndex)
            {
                if(0 > videoCodec.decodeVideo22(pFrame.get(), &frameFinished, pkt.get()))
		    break;

                if(0 < frameFinished)
                {
                    if(pFrame->interlaced_frame && fixme < 20)
                    {
                        DEBUG("FIXME interlaced frame"); fixme++;
                        //https://stackoverflow.com/questions/31163120/c-applying-filter-in-ffmpeg
                    }

                    sws_scale(ctxSws.get(), (uint8_t const* const*) pFrame->data, pFrame->linesize, 0,
                            videoCodec.height(), pFrameRGB->data, pFrameRGB->linesize);

                    // dump video frame
                    if(4 < debug)
                    {
                        VERBOSE("image width: " << videoCodec.width() << ", height: " << videoCodec.height() <<
                            ", data size: " << pFrameRGB->linesize[0] * videoCodec.height() << ", pixelFormat: " << "RGB24");
                    }

#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
                    // AV_PIX_FMT_0RGB -> SDL_PIXELFORMAT_BGRX8888
                    int bpp = 32; uint32_t bmask = 0xFF000000; uint32_t gmask = 0x00FF0000; uint32_t rmask = 0x0000FF00; uint32_t amask = 0;
#else
                    // AV_PIX_FMT_0RGB -> SDL_PIXELFORMAT_XRGB8888
                    int bpp = 32; uint32_t amask = 0; uint32_t rmask = 0x00FF0000; uint32_t gmask = 0x0000FF00; uint32_t bmask = 0x000000FF;
#endif
                    SDL_Surface* sf = SDL_CreateRGBSurfaceFrom(pFrameRGB->data[0],
                            videoCodec.width(), videoCodec.height(),
                            bpp, pFrameRGB->linesize[0], rmask, gmask, bmask, amask);

                    av_frame_unref(pFrame.get());
                    av_frame_unref(pFrameRGB.get());

		    result = Surface::copy(sf);
                    break;
                }
            }
        }

        return 0 < frameFinished;
    }
};

struct capture_ffmpeg_t
{
    int		        debug;
    int			streamIndex;
    size_t              framesPerSec;
    std::thread         thread;
    std::atomic<bool>   shutdown;

    std::unique_ptr<VideoFormat> videoFormat;
    std::unique_ptr<VideoCodec> videoCodec;

    Frames              frames;

    capture_ffmpeg_t() : debug(0), streamIndex(-1), framesPerSec(25), shutdown(false)
    {
    }

    ~capture_ffmpeg_t()
    {
	clear();
    }

    void listDevices(void)
    {
	// list format
        AVInputFormat* format = nullptr;
	while(nullptr != (format = av_input_video_device_next(format)))
	{
    	    DEBUG("available format: " << format->name << ", (" << format->long_name << ")");
    
    	    AVDeviceInfoList* device_list = nullptr;

    	    if(format->get_device_list &&
        	0 == avdevice_list_input_sources(format, nullptr, nullptr, &device_list))
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

    bool init(const std::string & ffmpegDevice, const std::string & ffmpegFormat, const JsonObject & config)
    {
	av_log_set_level(3 < debug ? AV_LOG_DEBUG : AV_LOG_ERROR);

#if LIBAVFORMAT_VERSION_MAJOR < 58
	av_register_all();
#endif
	avformat_network_init();
	avdevice_register_all();

	listDevices();

        videoFormat.reset(new VideoFormat());
        videoCodec.reset(new VideoCodec());

        if(! videoFormat->init(ffmpegFormat, ffmpegDevice, config))
    	    return false;

	DEBUG("open input: " << ffmpegDevice << " success");

        auto videoStream = videoFormat->findVideoStreamIndex();
	if(videoStream.first)
    	{
            streamIndex = videoStream.second;
            // return next
	    return videoCodec->init(*videoStream.first);
        }

    	return false;
    }

    void start(void)
    {
        shutdown = false;

        // frames loop
        thread = std::thread([st = this]
        {
            Surface frame;
            size_t delay = 10;
            size_t duration = 1000/st->framesPerSec;
            auto point = std::chrono::steady_clock::now();

            while(! st->shutdown)
            {
                if(5 < st->frames.size())
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(duration));
                    point = std::chrono::steady_clock::now();
                    continue;
                }

                if(st->videoFormat->frameToSurface(*st->videoCodec.get(), st->streamIndex, st->debug, frame))
                {
                    auto now = std::chrono::steady_clock::now();
                    auto timeMS = std::chrono::duration_cast<std::chrono::milliseconds>(now - point);

                    if(4 < st->debug)
                    {
                        DEBUG("real frame time: " << timeMS.count() << "ms");
                    }

                    if(timeMS > std::chrono::milliseconds(duration))
                    {
                        delay--;
                    }
                    else
                    if(timeMS < std::chrono::milliseconds(duration))
                    {
                        delay++;
                    }

                    st->frames.push_back(frame);
                    DisplayScene::pushEvent(nullptr, ActionFrameComplete, st);
                    point = now;
                }
                else
                {
                    DisplayScene::pushEvent(nullptr, ActionCaptureReset, st);
                    st->shutdown = true;
                }

                if(delay)
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
            }
        });
    }

    void clear(void)
    {
        shutdown = true;
        if(thread.joinable())
            thread.join();
	debug = 0;
	streamIndex = -1;
        framesPerSec = 25;
        videoCodec.reset();
        videoFormat.reset();
        frames.clear();
    }
};

void* capture_ffmpeg_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_ffmpeg_version <<
            ", avdevice: " << AV_STRINGIFY(LIBAVDEVICE_VERSION) << ", avformat: " << AV_STRINGIFY(LIBAVFORMAT_VERSION));

    auto ptr = std::make_unique<capture_ffmpeg_t>();
    ptr->debug = config.getInteger("debug", 0);
    ptr->framesPerSec = config.getInteger("frames:sec", 25);
    if(0 == ptr->framesPerSec || ptr->framesPerSec > 1000)
    {
        ERROR("frames:sec param incorrect, set default");
        ptr->framesPerSec = 25;
    }

    std::string ffmpegDevice = config.getString("device");
    std::string ffmpegFormat = config.getString("format");

    if(ffmpegDevice.empty())
    {
        ERROR("device param empty");
        ptr->clear();
        return nullptr;
    }

    DEBUG("params: " << "frames:sec = " << ptr->framesPerSec);
    DEBUG("params: " << "device = " << ffmpegDevice);
    if(! ffmpegFormat.empty())
        DEBUG("params: " << "format = " << ffmpegFormat);

    if(! ptr->init(ffmpegDevice, ffmpegFormat, config))
	return nullptr;

    ptr->start();

    return ptr.release();
}

void capture_ffmpeg_quit(void* ptr)
{
    capture_ffmpeg_t* st = static_cast<capture_ffmpeg_t*>(ptr);
    if(st->debug) VERBOSE("version: " << capture_ffmpeg_version);

    delete st;
}

bool capture_ffmpeg_get_value(void* ptr, int type, void* val)
{
    switch(type)
    {
        case PluginValue::PluginName:
            if(auto res = static_cast<std::string*>(val))
            {
                res->assign("capture_ffmpeg");
                return true;
            }
            break;
    
        case PluginValue::PluginVersion:
            if(auto res = static_cast<int*>(val))
            {
                *res = capture_ffmpeg_version;
                return true;
            }
            break;

        case PluginValue::PluginAPI:
            if(auto res = static_cast<int*>(val))
            {
                *res = PLUGIN_API;
                return true;
            }
            break;

        case PluginValue::PluginType:
            if(auto res = static_cast<int*>(val))
            {
                *res = PluginType::Capture;
                return true;
            }
            break;

        default:
            break;
    }

    if(ptr)
    {
        capture_ffmpeg_t* st = static_cast<capture_ffmpeg_t*>(ptr);
        if(4 < st->debug)
            DEBUG("version: " << capture_ffmpeg_version << ", type: " << type);

        switch(type)
        {
            case PluginValue::CaptureSurface:
                if(auto res = static_cast<Surface*>(val))
                {
                    if(! st->frames.empty())
                    {
                        res->setSurface(st->frames.front());
                        st->frames.pop_front();
                        return true;
                    }
                    return false;
                }
                break;

            default: break;
        }
    }

    return false;
}

bool capture_ffmpeg_set_value(void* ptr, int type, const void* val)
{
    capture_ffmpeg_t* st = static_cast<capture_ffmpeg_t*>(ptr);
    if(4 < st->debug)
        DEBUG("version: " << capture_ffmpeg_version << ", type: " << type);

    switch(type)
    {
        default: break;
    }

    return false;
}

#ifdef __cplusplus
}
#endif
