/***************************************************************************
 *   Copyright (C) 2022 by MultiCapture team <public.irkutsk@gmail.com>    *
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

#include <thread>
#include <atomic>
#include <chrono>

#include "../../settings.h"
#include "storage_format.h"

using namespace std::literals;

#ifdef __cplusplus
extern "C" {
#endif

const int storage_video_version = 20220415;

#include "libavdevice/avdevice.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
#include "libswscale/swscale.h"

struct AVCodecContextDeleter
{
    void operator()(AVCodecContext* ctx)
    {
        avcodec_free_context(& ctx);
    }
};

struct AVFormatContextDeleter
{
    void operator()(AVFormatContext* ctx)
    {
        avformat_free_context(ctx);
    }
};

struct SwsContextDeleter
{
    void operator()(SwsContext* ctx)
    {
        sws_freeContext(ctx);
    }
};

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

struct storage_video_t
{
    int         debug;
    int         videoLength;
    bool        isRecordMode;
    size_t      sessionId;
    std::atomic<bool> stopPushFrame;
    std::string sessionName;
    std::string filename;
    std::string format;
    std::string type;
    Size        geometry;

    std::chrono::time_point<std::chrono::steady_clock> startRecordPoint;
    std::thread pushFrameThread;
    Frames frames;

    AVOutputFormat* oformat;
    AVStream* stream;

    std::unique_ptr<AVCodecContext, AVCodecContextDeleter> avcctx;
    std::unique_ptr<AVFormatContext, AVFormatContextDeleter> avfctx;
    std::unique_ptr<SwsContext, SwsContextDeleter> swsctx;
    std::unique_ptr<AVFrame, AVFrameDeleter> ovframe;

    int fps;
    int frameCounter;

    storage_video_t() : debug(0), videoLength(0), isRecordMode(false), sessionId(0), stopPushFrame(false),
        type("avi"), oformat(nullptr), stream(nullptr), fps(25), frameCounter(0)
    {
        startRecordPoint = std::chrono::steady_clock::now();
    }

    ~storage_video_t()
    {
        if(isRecordMode) finish();
	clear();
    }

    int bitrate(void) const
    {
        return 1024;
    }

    bool init(void)
    {
        av_log_set_level(debug ? AV_LOG_DEBUG : AV_LOG_ERROR);

#if LIBAVFORMAT_VERSION_MAJOR < 58
        av_register_all();
        avcodec_register_all();
#endif
        oformat = av_guess_format(type.c_str(), nullptr, nullptr);
        if(! oformat)
        {
            ERROR("can't create output format");
            return false;
        }

        return init2();
    }

    bool init2(void)
    {
        AVFormatContext* avfctx2 = nullptr;

        int err = avformat_alloc_output_context2(& avfctx2, oformat, nullptr, nullptr);
        if(err < 0)
        {
            ERROR("can't create output context: " << err);
            return false;
        }

        avfctx.reset(avfctx2);

        AVCodec* codec = avcodec_find_encoder(oformat->video_codec);
        if(! codec)
        {
            ERROR("can't create codec");
            return false;
        }

        stream = avformat_new_stream(avfctx.get(), codec);
        if(! stream)
        {
            ERROR("can't find format");
            return false;
        }

        avcctx.reset(avcodec_alloc_context3(codec));
        if(! avcctx)
        {
            ERROR("can't create codec context");
            return false;
        }

#if LIBAVFORMAT_VERSION_MAJOR > 56
        auto codecpar = stream->codecpar;
#else
        auto codecpar = stream->codec;
#endif
        codecpar->codec_id = oformat->video_codec;
        codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
#if LIBAVFORMAT_VERSION_MAJOR > 56
        codecpar->format = AV_PIX_FMT_YUV420P;
#else
        codecpar->pix_fmt = AV_PIX_FMT_YUV420P;
#endif
        codecpar->bit_rate = bitrate() * 1000;
        stream->avg_frame_rate = (AVRational){fps, 1};

#if LIBAVFORMAT_VERSION_MAJOR > 56
        avcodec_parameters_to_context(avcctx.get(), codecpar);
#endif
        if(! geometry.isEmpty())
        {
            avcctx->width = geometry.w;
            avcctx->height = geometry.h;
        }

        avcctx->time_base = (AVRational){1, fps};
        avcctx->max_b_frames = 2;
        avcctx->gop_size = 12;
        avcctx->framerate = (AVRational){fps, 1};

        if(codecpar->codec_id == AV_CODEC_ID_H264)
        {
            av_opt_set(avcctx.get(), "preset", "ultrafast", 0);
        }
        else
        if(codecpar->codec_id == AV_CODEC_ID_H265)
        {
            av_opt_set(avcctx.get(), "preset", "ultrafast", 0);
        }
        else
        {
            av_opt_set_int(avcctx.get(), "lossless", 1, 0);
        }

        return true;
    }

    bool init_record(int width, int height, AVPixelFormat avPixelFormat)
    {
        filename = String::strftime(format);

        if(0 < sessionId)
            filename = String::replace(filename, "${sid}", sessionId);

        if(! sessionName.empty())
            filename = String::replace(filename, "${session}", sessionName);

        std::string dir = Systems::dirname(filename);

        if(! Systems::isDirectory(dir))
            Systems::makeDirectory(dir, 0775);

        AVCodec* codec = avcodec_find_encoder(oformat->video_codec);
        int err = 0;

#if LIBAVFORMAT_VERSION_MAJOR > 56
        auto codecpar = stream->codecpar;
#else
        auto codecpar = stream->codec;
#endif
        avcctx->pix_fmt = AV_PIX_FMT_YUV420P;
        avcctx->width = width;
        avcctx->height = height;

        codecpar->width = width;
        codecpar->height = height;

#if LIBAVFORMAT_VERSION_MAJOR > 56
        err = avcodec_parameters_to_context(avcctx.get(), codecpar);
        if(err < 0)
        {
            ERROR("Failed set parameters to context: " << err);
            return false;
        }

        err = avcodec_parameters_from_context(codecpar, avcctx.get());
        if(err < 0)
        {
            ERROR("Failed get parameters from context: " << err);
            return false;
        }
#endif

        err = avcodec_open2(avcctx.get(), codec, nullptr);
        if(err < 0)
        {
            ERROR("Failed to open codec: " << err);
            return false;
        }

        if(! (oformat->flags & AVFMT_NOFILE))
        {
            err = avio_open(& avfctx->pb, filename.c_str(), AVIO_FLAG_WRITE);
            if(err < 0)
            {
                ERROR("Failed to open file: " << filename << ", error: " << err);
                return false;
            }
        }

        err = avformat_write_header(avfctx.get(), nullptr);
        if(err < 0)
        {
            ERROR("Failed to write header: " << err);
            return false;
        }

        ovframe.reset(av_frame_alloc());
        ovframe->format = AV_PIX_FMT_YUV420P;
        ovframe->width = avcctx->width;
        ovframe->height = avcctx->height;

        err = av_frame_get_buffer(ovframe.get(), 32);
        if(err < 0)
        {
            ERROR("Failed to allocate picture: " << err);
            return false;
        }

        swsctx.reset(sws_getContext(width, height, avPixelFormat, ovframe->width, ovframe->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, 0, 0, 0));

        if(1 < debug)
        {
            av_dump_format(avfctx.get(), 0, filename.c_str(), 1);
            DEBUG("time base, den: " << stream->time_base.den  << ", num: " << stream->time_base.num);
        }

        startRecordPoint = std::chrono::steady_clock::now();
        frameCounter = 0;
        isRecordMode = true;

        if(1 < debug)
            DEBUG("start record: " << filename);

        return true;
    }

    bool push(const SDL_Surface* sf)
    {
        const uint8_t* data[1] = { (const uint8_t*) sf->pixels };
        int lines[1] = { sf->pitch };

        // From RGB to YUV
        sws_scale(swsctx.get(), data, lines, 0, sf->h, ovframe->data, ovframe->linesize);

        ovframe->pts = (frameCounter++) * stream->time_base.den / (stream->time_base.num * fps);

#if LIBAVFORMAT_VERSION_MAJOR > 56
        std::unique_ptr<AVPacket, AVPacketDeleter> pkt(av_packet_alloc());
#else
	AVPacket _pkt;
	av_init_packet(& _pkt);
        std::unique_ptr<AVPacket, AVPacketDeleter> pkt(& _pkt);
#endif

        pkt->flags |= AV_PKT_FLAG_KEY;

#if LIBAVFORMAT_VERSION_MAJOR > 56
        if(int err = avcodec_send_frame(avcctx.get(), ovframe.get()))
        {
            if(1 < debug)
            {
                char buf[1024];

                av_strerror(err, buf, sizeof(buf));
                ERROR("av_strerror: " << buf);
            }

            ERROR("Failed to send frame: " << err);
            return false;
        }

        if(0 == avcodec_receive_packet(avcctx.get(), pkt.get()))
            av_interleaved_write_frame(avfctx.get(), pkt.get());
#else
	int got_packet;
	if(int err = avcodec_encode_video2(avcctx.get(), pkt.get(), ovframe.get(), & got_packet))
	{
            if(1 < debug)
            {
                char buf[1024];

                av_strerror(err, buf, sizeof(buf));
                ERROR("av_strerror: " << buf);
            }

            ERROR("Failed to send frame: " << err);
            return false;
	}
	else
            av_interleaved_write_frame(avfctx.get(), pkt.get());
#endif
        return true;
    }

    void finish(void)
    {
        while(true)
        {
#if LIBAVFORMAT_VERSION_MAJOR > 56
            std::unique_ptr<AVPacket, AVPacketDeleter> pkt(av_packet_alloc());
#else
            AVPacket _pkt;
            av_init_packet(& _pkt);
            std::unique_ptr<AVPacket, AVPacketDeleter> pkt(& _pkt);
#endif

#if LIBAVFORMAT_VERSION_MAJOR > 56
            avcodec_send_frame(avcctx.get(), nullptr);

            if(0 != avcodec_receive_packet(avcctx.get(), pkt.get()))
                break;
#else
	    int got_packet;
	    if(0 != avcodec_encode_video2(avcctx.get(), pkt.get(), ovframe.get(), & got_packet))
                break;
#endif
            av_interleaved_write_frame(avfctx.get(), pkt.get());
        }

        av_write_trailer(avfctx.get());
    
        if(! (oformat->flags & AVFMT_NOFILE))
            avio_close(avfctx->pb);

        if(1 < debug)
            DEBUG("stop record: " << filename);

        frameCounter = 0;
        startRecordPoint = std::chrono::steady_clock::now();
        isRecordMode = false;
    }

    void clear(void)
    {
        stopPushFrame = true;
        if(pushFrameThread.joinable())
            pushFrameThread.join();

        if(isRecordMode)
            finish();

        debug = 0;
        filename.clear();
        format.clear();
	frames.clear();
        sessionName.clear();

        type = "avi";

        oformat = nullptr;
        stream = nullptr;
        startRecordPoint = std::chrono::steady_clock::now();

        ovframe.reset();
        swsctx.reset();
        avcctx.reset();
        avfctx.reset();

        fps = 25;
        sessionId = 0;
        frameCounter = 0;
        isRecordMode = false;
    }
};

void* storage_video_init(const JsonObject & config)
{
    VERBOSE("version: " << storage_video_version);

    auto ptr = std::make_unique<storage_video_t>();

    ptr->debug = config.getInteger("debug", 0);
    ptr->videoLength = config.getInteger("record:sec", 0);
    ptr->fps = config.getInteger("record:fps", 25);
    ptr->type = config.getString("record:format", "avi");
    ptr->format = config.getString("filename");
    ptr->geometry = JsonUnpack::size(config, "record:geometry");

    ptr->fps = config.getInteger("record:fps", 25);
    if(ptr->fps <= 0)
        ptr->fps = 25;

    if(ptr->format.empty())
    {
        if(ptr->format.empty())
            ERROR("filename param empty");
        ptr->clear();
        return nullptr;
    }

    if(0 > ptr->videoLength)
        ptr->videoLength = 0;

    DEBUG("params: " << "filename = " << ptr->format);
    DEBUG("params: " << "record:sec = " << ptr->videoLength);
    DEBUG("params: " << "record:format = " << ptr->type);
    DEBUG("params: " << "record:fps = " << ptr->fps);

    if(! ptr->geometry.isEmpty())
        DEBUG("params: " << "record:geometry = " << ptr->geometry.toString());

    if(! ptr->init())
    {
        ptr->clear();
        return nullptr;
    }

    return ptr.release();
}

void storage_video_quit(void* ptr)
{
    storage_video_t* st = static_cast<storage_video_t*>(ptr);
    if(st->debug) DEBUG("version: " << storage_video_version);

    delete st;
}

// PluginResult::Reset, PluginResult::Failed, PluginResult::DefaultOk, PluginResult::NoAction
int storage_video_store_action(void* ptr, const std::string & signal)
{
    storage_video_t* st = static_cast<storage_video_t*>(ptr);
    if(st->frames.empty())
    {
        ERROR("no frames");
        return PluginResult::Failed;
    }

    if(3 < st->debug) DEBUG("version: " << storage_video_version);

    if(! st->isRecordMode)
    {
        const SDL_Surface* sf = st->frames.front().toSDLSurface();
        auto avPixelFormat = AV_PixelFormatEnumFromMasks(sf->format->BitsPerPixel,
                                    sf->format->Rmask, sf->format->Gmask, sf->format->Bmask, sf->format->Amask, st->debug);

        if(avPixelFormat == AV_PIX_FMT_NONE)
        {
            ERROR("unknown pixel format");
            return PluginResult::Failed;
        }

        if(st->init_record(sf->w, sf->h, avPixelFormat))
        {
            // push frames job
            st->pushFrameThread = std::thread([st]{
                auto point = std::chrono::steady_clock::now();
                int duration = 1000 / st->fps;

                while(! st->stopPushFrame)
                {
                    if(st->frames.empty()) break;

                    auto now = std::chrono::steady_clock::now();

                    // push frame
                    if(std::chrono::milliseconds(duration) <= now - point)
                    {
                        auto sf = st->frames.front().toSDLSurface();
                        st->push(sf);

                        if(1 < st->frames.size())
                            st->frames.pop_front();

                        point = now;
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                st->stopPushFrame = true;
            });

            return PluginResult::DefaultOk;
        }

        return PluginResult::Reset;
    }
    else
    {
        st->stopPushFrame = true;
        if(st->pushFrameThread.joinable())
            st->pushFrameThread.join();

        // fixed video length
        if(0 < st->videoLength)
        {
            auto now = std::chrono::steady_clock::now();
            if(std::chrono::seconds(st->videoLength) <= now - st->startRecordPoint)
            {
                st->finish();
                return st->init2() ? PluginResult::NoAction : PluginResult::Reset;
            }
        }

        st->finish();
        return st->init2() ? PluginResult::NoAction : PluginResult::Reset;
    }

    return PluginResult::Failed;
}

bool storage_video_get_value(void* ptr, int type, void* val)
{
    switch(type)
    {
        case PluginValue::PluginName:
            if(auto res = static_cast<std::string*>(val))
            {
                res->assign("storage_video");
                return true;
            }
            break;

        case PluginValue::PluginVersion:
            if(auto res = static_cast<int*>(val))
            {
                *res = storage_video_version;
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
                *res = PluginType::Storage;
                return true;
            }
            break;

        default:
            break;
    }

    if(ptr)
    {
        storage_video_t* st = static_cast<storage_video_t*>(ptr);
        if(4 < st->debug)
            DEBUG("version: " << storage_video_version << ", type: " << type);

        switch(type)
        {
            case PluginValue::StorageLocation:
                if(auto res = static_cast<std::string*>(val))
                {
                    res->assign(st->filename);
                    return true;
                }
                break;

            case PluginValue::StorageSurface:
                if(auto res = static_cast<Surface*>(val))
                {
                    *res = Surface::copy(st->frames.back());
                    int cw = res->height() / 8;
                    res->fill(Rect(res->width() - cw - cw / 2, cw / 2, cw, cw), Color::Red);
                    return true;
                }
                break;

            default: break;
        }
    }

    return false;
}

bool storage_video_set_value(void* ptr, int type, const void* val)
{
    storage_video_t* st = static_cast<storage_video_t*>(ptr);
    if(4 < st->debug)
        DEBUG("version: " << storage_video_version << ", type: " << type);

    switch(type)
    {
        case PluginValue::StorageSurface:
            if(auto res = static_cast<const Surface*>(val))
            {
		if(! res->isValid())
		    return false;

                if(5 < st->frames.size())
                    return false;

                st->frames.push_back(*res);
                return true;
            }
            break;

        case PluginValue::SessionId:
            if(auto res = static_cast<const size_t*>(val))
            {
                st->sessionId = *res;
                return true;
            }
            break;

        case PluginValue::SessionName:
            if(auto res = static_cast<const std::string*>(val))
            {
                st->sessionName.assign(*res);
                return true;
            }
            break;

        default: break;
    }

    return false;
}

#ifdef __cplusplus
}
#endif
