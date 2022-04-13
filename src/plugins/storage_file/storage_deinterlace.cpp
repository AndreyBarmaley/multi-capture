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

#include <memory>
#include <exception>
#include <algorithm>

#include "libswe.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "libavformat/avformat.h"
#include "libswscale/swscale.h"


using namespace SWE;

int sd_pixelformat_from_av(AVPixelFormat avFormat)
{
    int sdlPixelFormat = SDL_PIXELFORMAT_UNKNOWN;
    switch(avFormat)
    {
        // tested little endian
        case AV_PIX_FMT_RGB24: sdlPixelFormat = SDL_PIXELFORMAT_RGB24; break;
        case AV_PIX_FMT_BGR24: sdlPixelFormat = SDL_PIXELFORMAT_BGR24; break;
        case AV_PIX_FMT_BGR0: sdlPixelFormat = SDL_PIXELFORMAT_RGB888; break;
        case AV_PIX_FMT_RGB0: sdlPixelFormat = SDL_PIXELFORMAT_BGR888; break;
        case AV_PIX_FMT_RGBA: sdlPixelFormat = SDL_PIXELFORMAT_ABGR8888; break;
        case AV_PIX_FMT_BGRA: sdlPixelFormat = SDL_PIXELFORMAT_ARGB8888; break;
        // untested
        case AV_PIX_FMT_ABGR: sdlPixelFormat = SDL_PIXELFORMAT_RGBA8888; break;
        case AV_PIX_FMT_ARGB: sdlPixelFormat = SDL_PIXELFORMAT_BGRA8888; break;
        case AV_PIX_FMT_0BGR: sdlPixelFormat = SDL_PIXELFORMAT_RGBX8888; break;
        case AV_PIX_FMT_0RGB: sdlPixelFormat = SDL_PIXELFORMAT_BGRX8888; break;
        default: ERROR("unknown av format: " << avFormat); break;
    }
    return sdlPixelFormat;
}

AVPixelFormat av_pixelformat_from_sdl(int sdlFormat)
{
    AVPixelFormat avPixelFormat = AV_PIX_FMT_NONE;
    switch(sdlFormat)
    {
        // tested little endian
        case SDL_PIXELFORMAT_RGB24: avPixelFormat = AV_PIX_FMT_RGB24; break;
        case SDL_PIXELFORMAT_BGR24: avPixelFormat = AV_PIX_FMT_BGR24; break;
        case SDL_PIXELFORMAT_RGB888: avPixelFormat = AV_PIX_FMT_BGR0; break;
        case SDL_PIXELFORMAT_BGR888: avPixelFormat = AV_PIX_FMT_RGB0; break;
        case SDL_PIXELFORMAT_ABGR8888: avPixelFormat = AV_PIX_FMT_RGBA; break;
        case SDL_PIXELFORMAT_ARGB8888: avPixelFormat = AV_PIX_FMT_BGRA; break;
        // untested
        case SDL_PIXELFORMAT_RGBA8888: avPixelFormat = AV_PIX_FMT_ABGR; break;
        case SDL_PIXELFORMAT_BGRA8888: avPixelFormat = AV_PIX_FMT_ARGB; break;
        case SDL_PIXELFORMAT_RGBX8888: avPixelFormat = AV_PIX_FMT_0BGR; break;
        case SDL_PIXELFORMAT_BGRX8888: avPixelFormat = AV_PIX_FMT_0RGB; break;
        default: ERROR("unknown SDL format: " << SDL_GetPixelFormatName(sdlFormat)); break;
    }
    return avPixelFormat;
}

struct AVFrameDeleter
{
    void operator()(AVFrame* frame)
    {
        av_frame_free(& frame);
    }
};

class SwFrame : public std::unique_ptr<AVFrame, AVFrameDeleter>
{
public:
    SwFrame() = default;

    SwFrame(int width, int height, AVPixelFormat format)
    {
        init(width, height, format);
    }

    SwFrame(const SDL_Surface* sf)
    {
        init(sf);
    }

    SwFrame(const Surface & sf)
    {
        init(sf.toSDLSurface());
    }

    void init(const SDL_Surface* sf)
    {
        auto sfpf = sf->format;
        auto sdlFormat = SDL_MasksToPixelFormatEnum(sfpf->BitsPerPixel, sfpf->Rmask, sfpf->Gmask, sfpf->Bmask, sfpf->Amask);
        //DEBUG("SDL format: " << SDL_GetPixelFormatName(sdlFormat));

        auto avPixelFormat = av_pixelformat_from_sdl(sdlFormat);
        if(avPixelFormat == AV_PIX_FMT_NONE)
            throw std::runtime_error(std::string("av_pixelformat_from_sdl failed, error: ").append("unknown pixel format"));

        init(sf->w, sf->h, avPixelFormat);

        get()->data[0] = (uint8_t*) sf->pixels;
        get()->linesize[0] = sf->pitch;
    }

    void init(int width, int height, AVPixelFormat format)
    {
        auto frame = av_frame_alloc();
        if(! frame)
            throw std::runtime_error("av_frame_alloc failed");

        frame->format = format;
        frame->width = width;
        frame->height = height;

        int err = av_frame_get_buffer(frame, 32);
        if(err < 0)
            throw std::runtime_error(std::string("av_frame_get_buffer failed, error: ").append(std::to_string(err)));

        reset(frame);
    }

    Surface toSurface(void) const
    {
        auto frame = get();

        int sdlFormat = sd_pixelformat_from_av((AVPixelFormat) frame->format);
        int sdlBpp = SDL_BITSPERPIXEL(sdlFormat);

        SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(frame->data[0],
                            frame->width, frame->height, sdlBpp, frame->linesize[0], sdlFormat);

        return Surface::copy(sf);
    }
};

SwFrame scaleFrame(const SwFrame & ivframe, int width, int height, AVPixelFormat format, int flag = 0)
{
    av_log_set_level(AV_LOG_DEBUG);
    SwFrame ovframe(width, height, format);

    auto sws = sws_getContext(ivframe->width, ivframe->height, (AVPixelFormat) ivframe->format,
                                ovframe->width, ovframe->height, (AVPixelFormat) ovframe->format, flag, 0, 0, 0);
    if(! sws)
        throw std::runtime_error("sws_getContext failed");

    sws_scale(sws, ivframe->data, ivframe->linesize, 0, ivframe->height, ovframe->data, ovframe->linesize);
    sws_freeContext(sws);

    return ovframe;
}

Surface ffmpegDeinterlace1(const Surface & back)
{
    SwFrame frame1(back);
    SwFrame frame2 = scaleFrame(frame1, frame1->width, frame1->height / 2, (AVPixelFormat) frame1->format, 0);
    return scaleFrame(frame2, frame2->width, frame2->height * 2, (AVPixelFormat) frame2->format, SWS_BICUBIC).toSurface();
}

Surface ffmpegScale(const Surface & back, const Size & sz)
{
    SwFrame frame1(back);
    return scaleFrame(frame1, sz.w, sz.h, (AVPixelFormat) frame1->format, SWS_BICUBIC).toSurface();
}

Surface ffmpegDeinterlace2(const Surface & back)
{
    auto sf1 = back.toSDLSurface();
    auto sf2 = SDL_CreateRGBSurface(0, sf1->w, sf1->h / 2, sf1->format->BitsPerPixel,
                                    sf1->format->Rmask, sf1->format->Gmask, sf1->format->Bmask, sf1->format->Amask);
    if(! sf2)
        throw std::runtime_error(std::string("SDL_CreateSurface, error: ").append(SDL_GetError()));

    for(int rows = 0; rows < sf2->h; ++rows)
    {
        auto src = (uint8_t*) sf1->pixels + sf1->pitch * rows * 2;
        auto dst = (uint8_t*) sf2->pixels + sf2->pitch * rows;
        std::copy_n(src, sf1->pitch, dst);
    }

    SwFrame frame1(sf2);
    SwFrame frame2 = scaleFrame(frame1, frame1->width, frame1->height * 2, (AVPixelFormat) frame1->format, SWS_BICUBIC);
    SDL_FreeSurface(sf2);

    return frame2.toSurface();
}

#ifdef __cplusplus
}
#endif
