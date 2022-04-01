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

#ifndef _LIBVNC_
#define _LIBVNC_

#include <list>
#include "libswe.h"

namespace Tools
{
    struct StreamBits
    {
        std::vector<uint8_t> vecbuf;
        size_t               bitpos;

        StreamBits() : bitpos(0) {}

        bool empty(void) const;
        const std::vector<uint8_t> & toVector(void) const;
    };

    struct StreamBitsPack : StreamBits
    {
        StreamBitsPack();

        void pushBit(bool v);
        void pushValue(int val, size_t field);
        void pushAlign(void);
    };

    struct StreamBitsUnpack : StreamBits
    {
        StreamBitsUnpack(const std::vector<uint8_t> &, size_t counts, size_t field);

        bool popBit(void);
        int popValue(size_t field);
    };
}

namespace TLS
{
    std::vector<uint8_t> encryptDES(const std::vector<uint8_t> & data, const std::string & str);
    std::vector<uint8_t> randomKey(size_t keysz);
}

namespace RFB
{
    // RFB protocol constant
    const int VERSION_MAJOR = 3;
    const int VERSION_MINOR = 8;

    const int SECURITY_TYPE_NONE = 1;
    const int SECURITY_TYPE_VNC = 2;
    const int SECURITY_TYPE_TLS = 18;
    const int SECURITY_TYPE_VENCRYPT = 19;
    const int SECURITY_VENCRYPT01_PLAIN = 19;
    const int SECURITY_VENCRYPT01_TLSNONE = 20;
    const int SECURITY_VENCRYPT01_TLSVNC = 21;
    const int SECURITY_VENCRYPT01_TLSPLAIN = 22;
    const int SECURITY_VENCRYPT01_X509NONE = 23;
    const int SECURITY_VENCRYPT01_X509VNC = 24;
    const int SECURITY_VENCRYPT01_X509PLAIN = 25;
    const int SECURITY_VENCRYPT02_PLAIN = 256;
    const int SECURITY_VENCRYPT02_TLSNONE = 257;
    const int SECURITY_VENCRYPT02_TLSVNC = 258;
    const int SECURITY_VENCRYPT02_TLSPLAIN = 259;
    const int SECURITY_VENCRYPT02_X509NONE = 260;
    const int SECURITY_VENCRYPT02_X509VNC = 261;
    const int SECURITY_VENCRYPT02_X509PLAIN = 261;

    const int SECURITY_RESULT_OK = 0;
    const int SECURITY_RESULT_ERR = 1;

    const int CLIENT_SET_PIXEL_FORMAT = 0;
    const int CLIENT_SET_ENCODINGS = 2;
    const int CLIENT_REQUEST_FB_UPDATE = 3;
    const int CLIENT_EVENT_KEY = 4;
    const int CLIENT_EVENT_POINTER = 5;
    const int CLIENT_CUT_TEXT = 6;
    const int CLIENT_ENABLE_CONTINUOUS_UPDATES = 150;
    const int CLIENT_SET_DESKTOP_SIZE = 251;

    const int SERVER_FB_UPDATE = 0;
    const int SERVER_SET_COLOURMAP = 1;
    const int SERVER_BELL = 2;
    const int SERVER_CUT_TEXT = 3;

    // RFB protocol constants
    const int ENCODING_RAW = 0;
    const int ENCODING_COPYRECT = 1;
    const int ENCODING_RRE = 2;
    const int ENCODING_CORRE = 4;
    const int ENCODING_HEXTILE = 5;
    const int ENCODING_ZLIB = 6;
    const int ENCODING_TIGHT = 7;
    const int ENCODING_ZLIBHEX = 8;
    const int ENCODING_TRLE = 15;
    const int ENCODING_ZRLE = 16;

    // hextile constants
    const int HEXTILE_RAW = 1;
    const int HEXTILE_BACKGROUND = 2;
    const int HEXTILE_FOREGROUND = 4;
    const int HEXTILE_SUBRECTS = 8;
    const int HEXTILE_COLOURED = 16;
    const int HEXTILE_ZLIBRAW = 32;
    const int HEXTILE_ZLIB = 64;

    // pseudo encodings
    const int ENCODING_DESKTOP_SIZE = -223;
    const int ENCODING_EXT_DESKTOP_SIZE = -308;
    const int ENCODING_CONTINUOUS_UPDATES = -313;
    const int ENCODING_LAST_RECT = -224;
    const int ENCODING_COMPRESS9 = -247;
    const int ENCODING_COMPRESS8 = -248;
    const int ENCODING_COMPRESS7 = -249;
    const int ENCODING_COMPRESS6 = -250;
    const int ENCODING_COMPRESS5 = -251;
    const int ENCODING_COMPRESS4 = -252;
    const int ENCODING_COMPRESS3 = -253;
    const int ENCODING_COMPRESS2 = -254;
    const int ENCODING_COMPRESS1 = -255;

    std::string encodingName(int type);

    struct Point
    {
        int16_t x, y;

        Point(int16_t px, int16_t py) : x(px), y(py) {}
        Point(const Point & pt) : x(pt.x), y(pt.y) {}
        Point() : x(-1), y(-1) {}

        Point operator+ (const Point & pt) const { return Point(x + pt.x, y + pt.y); }
        Point operator- (const Point & pt) const { return Point(x - pt.x, y - pt.y); }
     };

    struct Size
    {
        uint16_t w, h;

        Size(uint16_t pw, uint16_t ph) : w(pw), h(ph) {}
        Size(const Size & sz) : w(sz.w), h(sz.h) {}
        Size() : w(0), h(0) {}

        bool isEmpty(void) const { return w == 0 || h == 0; }
    };

    struct Region : public Point, public Size
    {
        Region(const Point & pt, const Size & sz) : Point(pt), Size(sz) {}
        Region(int16_t rx, int16_t ry, uint16_t rw, uint16_t rh) : Point(rx, ry), Size(rw, rh) {}
        Region() {}

        const Point &   toPoint(void) const { return *this; }
        const Size &    toSize(void) const { return *this; }

        Point           topLeft(void) const { return Point(x, y); }
        Point           topRight(void) const { return Point(x + w - 1, y); }

        Region operator+ (const Point & pt) const { return Region(x + pt.x, y + pt.y, w, h); }
        Region operator- (const Point & pt) const { return Region(x - pt.x, y - pt.y, w, h); }

        Region          intersected(const Region & reg) const;

        static std::list<Region> divideBlocks(const Region & rt, const Size &);
        static std::list<Region> divideCounts(const Region & rt, uint16_t cols, uint16_t rows);
        static bool              intersects(const Region &, const Region &);
        static bool              intersection(const Region &, const Region &, Region* res);
    };

    struct RegionPixel : std::pair<Region, uint32_t>
    {
        RegionPixel(const Region & reg, uint32_t pixel) : std::pair<Region, uint32_t>(reg, pixel) {}
        RegionPixel() {}

        const uint32_t &   pixel(void) const { return second; }
        const Region &     region(void) const { return first; }
    };

    struct Color
    {
        uint8_t             r, g, b, x;

        Color() : r(0), g(0), b(0), x(0) {}
        Color(uint8_t cr, uint8_t cg, uint8_t cb) : r(cr), g(cg), b(cb), x(0) {}

        int toRGB888(void) const { return (static_cast<int>(r) << 16) | (static_cast<int>(g) << 8) | b; }

        bool operator== (const Color & col) const { return r == col.r && g == col.g && b == col.b; }
        bool operator!= (const Color & col) const { return r != col.r || g != col.g || b != col.b; }
    };

    struct HasherColor
    {
        size_t operator()(const Color & col) const
        {
            return std::hash<size_t>()(col.toRGB888());
        }
    };

    struct PixelMapWeight : swe_unordered_map<uint32_t /* pixel */, uint32_t /* weight */>
    {
        int             maxWeightPixel(void) const;
    };

    struct PixelFormat
    {
        uint8_t         bitsPerPixel;
        uint8_t         depth;
        uint8_t         flags;

        uint8_t         redShift;
        uint8_t         greenShift;
        uint8_t         blueShift;

        uint16_t        redMax;
        uint16_t        greenMax;
        uint16_t        blueMax;

        enum { BigEndian = 0x01, TrueColor = 0x02 };

        PixelFormat() : bitsPerPixel(0), depth(0), flags(0), redShift(0),
            greenShift(0), blueShift(0), redMax(0), greenMax(0), blueMax(0) {}

        PixelFormat(int bpp, int dep, bool be, bool trucol, int rmask, int gmask, int bmask);
        PixelFormat(int bpp, int dep, bool be, bool trucol, int rmax, int gmax, int bmax, int rshift, int gshift, int bshift);

        bool operator!= (const PixelFormat & pf) const;

        uint32_t        rmask(void) const;
        uint32_t        gmask(void) const;
        uint32_t        bmask(void) const;

        bool            bigEndian(void) const;
        bool            trueColor(void) const;

        uint32_t        red(int pixel) const;
        uint32_t        green(int pixel) const;
        uint32_t        blue(int pixel) const;

        uint32_t        bytePerPixel(void) const;
        Color           color(int pixel) const;
        uint32_t        pixel(const Color & col) const;
        uint32_t        convertFrom(const PixelFormat & pf, uint32_t pixel) const;
    };

    struct fbinfo_t
    {
        bool            allocated;
        uint32_t        pitch;
        uint8_t*        buffer;
        PixelFormat     format;

        fbinfo_t(const Size &, const PixelFormat & fmt);
        fbinfo_t(uint8_t* ptr, const Size &, const PixelFormat & fmt);
        ~fbinfo_t();
    };

    struct PixelLength : std::pair<uint32_t /* pixel */, uint32_t /* length */>
    {
        PixelLength(uint32_t pixel, uint32_t length) : std::pair<uint32_t, uint32_t>(pixel, length) {}

        const uint32_t & pixel(void) const { return first; }
        const uint32_t & length(void) const { return second; }
    };

    struct PointIterator : Point
    {
        Size            limit;
        PointIterator(int px, int py, const Size & sz) : Point(px, py), limit(sz) {}

        PointIterator & operator++(void);
        PointIterator & operator--(void);

        bool            isValid(void) const
        {
            return 0 <= x && 0 <= y && x < limit.w && y < limit.h;
        }
    };

    class FrameBuffer
    {
        std::shared_ptr<fbinfo_t> fbptr;
        Region          fbreg;
        bool            owner;

    public:
        FrameBuffer(const Region & reg, const FrameBuffer & fb);
        FrameBuffer(const Size & rsz, const PixelFormat & fmt, size_t pitch = 0);
        FrameBuffer(uint8_t* p, const Region & reg, const PixelFormat & fmt, size_t pitch = 0);

        PointIterator   coordBegin(void) const { return PointIterator(0, 0, fbreg.toSize()); }

        void            setPixelRow(const Point &, uint32_t pixel, size_t length);

        void            setPixel(const Point &, uint32_t pixel, const PixelFormat* = nullptr);
        void            setColor(const Point &, const Color &);

        void            fillPixel(const Region &, uint32_t pixel, const PixelFormat* = nullptr);
        void            fillColor(const Region &, const Color &);
        void            drawRect(const Region &, const Color &);
        void            blitRegion(const FrameBuffer &, const Region &, const Point &);

        uint32_t        pixel(const Point &) const;

        PixelMapWeight  pixelMapWeight(const Region &) const;
        std::list<PixelLength> toRLE(const Region &) const;
        bool            allOfPixel(uint32_t pixel, const Region &) const;

        size_t          width(void) const;
        size_t          height(void) const;
        size_t          pitchSize(void) const;
        uint8_t*        pitchData(size_t row) const;

        const Region  & region(void) const { return fbreg; }

        Color           color(const Point &) const;
        uint32_t        bytePerPixel(void) const;
        uint32_t        bitsPerPixel(void) const;

        const PixelFormat & pixelFormat(void) const;
    };
}

#endif
