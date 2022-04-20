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

#include <algorithm>

#include "libswe.h"
#include "storage_format.h"

using namespace SWE;

bool AV_PixelFormatEnumToMasks(AVPixelFormat format, int *bpp, uint32_t* rmask, uint32_t* gmask, uint32_t* bmask, uint32_t* amask, bool debug)
{
    switch(format)
    {
        case AV_PIX_FMT_RGB24:
            if(debug) DEBUG("AV_PIX_FMT_RGB24");
            *bpp = 24; *amask = 0; *rmask = 0x00FF0000; *gmask = 0x0000FF00; *bmask = 0x000000FF;
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            std::swap(*rmask, *bmask);
#endif
            return true;

        case AV_PIX_FMT_BGR24:
            if(debug) DEBUG("AV_PIX_FMT_BGR24");
            *bpp = 24; *amask = 0; *bmask = 0x00FF0000; *gmask = 0x0000FF00; *rmask = 0x000000FF;
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            std::swap(*rmask, *bmask);
#endif
            return true;

        case AV_PIX_FMT_RGB0:
            if(debug) DEBUG("AV_PIX_FMT_RGB0");
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            *bpp = 32; *amask = 0; *bmask = 0x00FF0000; *gmask = 0x0000FF00; *rmask = 0x000000FF;
#else
            *bpp = 32; *rmask = 0xFF000000; *gmask = 0x00FF0000; *bmask = 0x0000FF00; *amask = 0;
#endif
            return true;

        case AV_PIX_FMT_0BGR:
            if(debug) DEBUG("AV_PIX_FMT_0BGR");
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            *bpp = 32; *rmask = 0xFF000000; *gmask = 0x00FF0000; *bmask = 0x0000FF00; *amask = 0;
#else
            *bpp = 32; *amask = 0; *bmask = 0x00FF0000; *gmask = 0x0000FF00; *rmask = 0x000000FF;
#endif
            return true;

        case AV_PIX_FMT_BGR0:
            if(debug) DEBUG("AV_PIX_FMT_BGR0");
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            *bpp = 32; *amask = 0; *rmask = 0x00FF0000; *gmask = 0x0000FF00; *bmask = 0x000000FF;
#else
            *bpp = 32; *bmask = 0xFF000000; *gmask = 0x00FF0000; *rmask = 0x0000FF00; *amask = 0;
#endif
            return true;

        case AV_PIX_FMT_0RGB:
            if(debug) DEBUG("AV_PIX_FMT_0RGB");
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            *bpp = 32; *bmask = 0xFF000000; *gmask = 0x00FF0000; *rmask = 0x0000FF00; *amask = 0;
#else
            *bpp = 32; *amask = 0; *rmask = 0x00FF0000; *gmask = 0x0000FF00; *bmask = 0x000000FF;
#endif
            return true;

        case AV_PIX_FMT_RGBA:
            if(debug) DEBUG("AV_PIX_FMT_RGBA");
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            *bpp = 32; *amask = 0xFF000000; *bmask = 0x00FF0000; *gmask = 0x0000FF00; *rmask = 0x000000FF;
#else
            *bpp = 32; *rmask = 0xFF000000; *gmask = 0x00FF0000; *bmask = 0x0000FF00; *amask = 0x000000FF;
#endif
            return true;

        case AV_PIX_FMT_ABGR:
            if(debug) DEBUG("AV_PIX_FMT_ABGR");
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            *bpp = 32; *rmask = 0xFF000000; *gmask = 0x00FF0000; *bmask = 0x0000FF00; *amask = 0x000000FF;
#else
            *bpp = 32; *amask = 0xFF000000; *bmask = 0x00FF0000; *gmask = 0x0000FF00; *rmask = 0x000000FF;
#endif
            return true;

        case AV_PIX_FMT_BGRA:
            if(debug) DEBUG("AV_PIX_FMT_BGRA");
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            *bpp = 32; *amask = 0xFF000000; *rmask = 0x00FF0000; *gmask = 0x0000FF00; *bmask = 0x000000FF;
#else
            *bpp = 32; *bmask = 0xFF000000; *gmask = 0x00FF0000; *rmask = 0x0000FF00; *amask = 0x000000FF;
#endif
            return true;


        case AV_PIX_FMT_ARGB:
            if(debug) DEBUG("AV_PIX_FMT_ARGB");
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            *bpp = 32; *bmask = 0xFF000000; *gmask = 0x00FF0000; *rmask = 0x0000FF00; *amask = 0x000000FF;
#else
            *bpp = 32; *amask = 0xFF000000; *rmask = 0x00FF0000; *gmask = 0x0000FF00; *bmask = 0x000000FF;
#endif
            return true;

        default:
            break;
    }

    return false;
}

AVPixelFormat AV_PixelFormatEnumFromMasks(int bpp, uint32_t rmask, uint32_t gmask, uint32_t bmask, uint32_t amask, bool debug)
{
    if(debug)
    {
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
	bool bigEndian = false;
#else
	bool bigEndian = true;
#endif
	DEBUG("pixel format, bpp: " << bpp << ", rmask: " << String::hex(rmask, 8) << ", gmask: " << String::hex(gmask, 8) <<
	    ", bmask: " << String::hex(bmask, 8) << ", amask: " << String::hex(amask, 8) << ", be: " << String::Bool(bigEndian));
    }
    
    if(24 == bpp)
    {
        if(amask == 0 && rmask == 0x00FF0000 && gmask == 0x0000FF00 && bmask == 0x000000FF)
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            return AV_PIX_FMT_BGR24;
#else
            return AV_PIX_FMT_RGB24;
#endif

        if(amask == 0 && bmask == 0x00FF0000 && gmask == 0x0000FF00 && rmask == 0x000000FF)
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            return AV_PIX_FMT_RGB24;
#else
            return AV_PIX_FMT_BGR24;
#endif
    }
    else
    if(32 == bpp)
    {
        if(rmask == 0xFF000000 && gmask == 0x00FF0000 && bmask == 0x0000FF00 && amask == 0)
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            return AV_PIX_FMT_0BGR;
#else
            return AV_PIX_FMT_RGB0;
#endif

        if(amask == 0 && bmask == 0x00FF0000 && gmask == 0x0000FF00 && rmask == 0x000000FF)
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            return AV_PIX_FMT_RGB0;
#else
            return AV_PIX_FMT_0BGR;
#endif

        if(bmask == 0xFF000000 && gmask == 0x00FF0000 && rmask == 0x0000FF00 && amask == 0)
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            return AV_PIX_FMT_0RGB;
#else
            return AV_PIX_FMT_BGR0;
#endif

        if(amask == 0 && rmask == 0x00FF0000 && gmask == 0x0000FF00 && bmask == 0x000000FF)
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            return AV_PIX_FMT_BGR0;
#else
            return AV_PIX_FMT_0RGB;
#endif

        if(rmask == 0xFF000000 && gmask == 0x00FF0000 && bmask == 0x0000FF00 && amask == 0x000000FF)
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            return AV_PIX_FMT_ABGR;
#else
            return AV_PIX_FMT_RGBA;
#endif

        if(amask == 0xFF000000 && bmask == 0x00FF0000 && gmask == 0x0000FF00 && rmask == 0x000000FF)
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            return AV_PIX_FMT_RGBA;
#else
            return AV_PIX_FMT_ABGR;
#endif

        if(bmask == 0xFF000000 && gmask == 0x00FF0000 && rmask == 0x0000FF00 && amask == 0x000000FF)
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            return AV_PIX_FMT_ARGB;
#else
            return AV_PIX_FMT_BGRA;
#endif

        if(amask == 0xFF000000 && rmask == 0x00FF0000 && gmask == 0x0000FF00 && bmask == 0x000000FF)
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            return AV_PIX_FMT_BGRA;
#else
            return AV_PIX_FMT_ARGB;
#endif
    }

    ERROR(StringFormat("unsupported pixel format, bpp: %1, rmask: %2, gmask: %3, bmask: %4, amask: %2").
            arg(bpp).arg(String::hex(rmask, 8)).arg(String::hex(gmask, 8)).arg(String::hex(bmask, 8)).arg(String::hex(amask, 8)));

    return AV_PIX_FMT_NONE;
}
