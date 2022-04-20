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

#include <cctype>
#include <exception>
#include <algorithm>

#include "gnutls/gnutls.h"
#include "gnutls/crypto.h"

#include "libvnc.h"

namespace TLS
{
    std::vector<uint8_t> encryptDES(const std::vector<uint8_t> & data, const std::string & str)
    {
        gnutls_cipher_hd_t ctx;
        std::vector<uint8_t> res(data);
        std::array<uint8_t, 8> _key = { 0 };
        std::array<uint8_t, 8> _iv = { 0 };
        std::copy_n(str.begin(), std::min(str.size(), _key.size()), _key.begin());
        gnutls_datum_t key = { _key.data(), _key.size() };
        gnutls_datum_t iv = { _iv.data(), _iv.size() };

        // Reverse the order of bits in the byte
        for(auto & val : _key)
            if(val) val = ((val * 0x0202020202ULL & 0x010884422010ULL) % 1023) & 0xfe;

        size_t offset = 0;

        while(offset < res.size())
        {
            if(int ret = gnutls_cipher_init(& ctx, GNUTLS_CIPHER_DES_CBC, & key, & iv))
                throw std::runtime_error(std::string("gnutls_cipher_init: ").append(gnutls_strerror(ret)));

            if(int ret = gnutls_cipher_encrypt(ctx, res.data() + offset, std::min(_key.size(), res.size() - offset)))
                throw std::runtime_error(std::string("gnutls_cipher_encrypt: ").append(gnutls_strerror(ret)));

            gnutls_cipher_deinit(ctx);
            offset += _key.size();
        }

        return res;
    }

    std::vector<uint8_t> randomKey(size_t keysz)
    {
        std::vector<uint8_t> res(keysz);
    
        if(int ret = gnutls_rnd(GNUTLS_RND_KEY, res.data(), res.size()))
            throw std::runtime_error(std::string("gnutls_rnd:").append(gnutls_strerror(ret)));

        return res;
    }
} // TLS

namespace Tools
{
    /* StreamBits */
    bool StreamBits::empty(void) const
    {
        return vecbuf.empty() ||
                (vecbuf.size() == 1 && bitpos == 7);
    }

    const std::vector<uint8_t> & StreamBits::toVector(void) const
    {
        return vecbuf;
    }

    /* StreamBitsPack */
    StreamBitsPack::StreamBitsPack()
    {
        bitpos = 7;
        vecbuf.reserve(32);
    }
    void StreamBitsPack::pushBit(bool v)
    {
        if(bitpos == 7)
            vecbuf.push_back(0);

        uint8_t mask = 1 << bitpos;
        if(v) vecbuf.back() |= mask;
            
        if(bitpos == 0)
            bitpos = 7;
        else
            bitpos--;
    }

    void StreamBitsPack::pushAlign(void)
    {
        bitpos = 7;
    }

    void StreamBitsPack::pushValue(int val, size_t field)
    {
        // field 1: mask 0x0001, field 2: mask 0x0010, field 4: mask 0x1000
        size_t mask = 1 << (field - 1);

        while(mask)
        {
            pushBit(val & mask);
            mask >>= 1;
        }
    }

    /* StreamBitsUnpack */
    StreamBitsUnpack::StreamBitsUnpack(const std::vector<uint8_t> & v, size_t counts, size_t field)
    {
        // check size
        size_t bits = field * counts;
        size_t len = bits >> 3;
        if((len << 3) < bits) len++;

        if(len < v.size())
            throw std::runtime_error("stream bits: incorrect data size");

        vecbuf.assign(v.begin(), v.end());
        bitpos = (len << 3) - bits;
    }

    bool StreamBitsUnpack::popBit(void)
    {
        if(vecbuf.empty())
            throw std::runtime_error("stream bits: empty data");

        uint8_t mask = 1 << bitpos;
        bool res = vecbuf.back() & mask;

        if(bitpos == 7)
        {
            vecbuf.pop_back();
            bitpos = 0;
        }
        else
        {
            bitpos++;
        }

        return res;
    }

    int StreamBitsUnpack::popValue(size_t field)
    {
        // field 1: mask 0x0001, field 2: mask 0x0010, field 4: mask 0x1000
        size_t mask1 = 1 << (field - 1);
        size_t mask2 = 1;
        int val = 0;

        while(mask1)
        {
            if(popBit())
                val |= mask2;

            mask1 >>= 1;
            mask2 <<= 1;
        }

        return val;
    }

    //
    size_t maskShifted(size_t mask)
    {
        size_t res = 0;

        if(mask)
        {
            while((mask & 1) == 0)
            {
                mask = mask >> 1;
                res = res + 1;
            }
        }

        return res;
    }

    size_t maskMaxValue(uint32_t mask)
    {
        size_t res = 0;

        if(mask)
        {
            while((mask & 1) == 0)
                mask = mask >> 1;

            res = ~static_cast<size_t>(0) & mask;
        }

        return res;
    }
}

namespace RFB
{
    std::string encodingName(int type)
    {
        switch(type)
        {
            case ENCODING_RAW:
                return "Raw";

            case ENCODING_COPYRECT:
                return "CopyRect";

            case ENCODING_RRE:
                return "RRE";

            case ENCODING_CORRE:
                return "CoRRE";

            case ENCODING_HEXTILE:
                return "HexTile";

            case ENCODING_ZLIB:
                return "ZLib";

            case ENCODING_TIGHT:
                return "Tight";

            case ENCODING_ZLIBHEX:
                return "ZLibHex";

            case ENCODING_TRLE:
                return "TRLE";

            case ENCODING_ZRLE:
                return "ZRLE";

            case ENCODING_DESKTOP_SIZE:
                return "DesktopSize";

            case ENCODING_EXT_DESKTOP_SIZE:
                return "ExtendedDesktopSize";

            case ENCODING_LAST_RECT:
                return "ExtendedLastRect";

            case ENCODING_COMPRESS9:
                return "ExtendedCompress9";

            case ENCODING_COMPRESS8:
                return "ExtendedCompress8";

            case ENCODING_COMPRESS7:
                return "ExtendedCompress7";

            case ENCODING_COMPRESS6:
                return "ExtendedCompress6";

            case ENCODING_COMPRESS5:
                return "ExtendedCompress5";

            case ENCODING_COMPRESS4:
                return "ExtendedCompress4";

            case ENCODING_COMPRESS3:
                return "ExtendedCompress3";

            case ENCODING_COMPRESS2:
                return "ExtendedCompress2";

            case ENCODING_COMPRESS1:
                return "ExtendedCompress1";

            case ENCODING_CONTINUOUS_UPDATES:
                return "ExtendedContinuousUpdates";

            default:
                break;
        }

        return SWE::String::hex(type, 8);
    }

    /* Region */
    std::list<Region> Region::divideBlocks(const Region & rt, const Size & sz)
    {
        std::list<Region> res;

        int cw = sz.w > rt.w ? rt.w : sz.w;
        int ch = sz.h > rt.h ? rt.h : sz.h;

        for(uint16_t yy = 0; yy < rt.h; yy += ch)
        {
            for(uint16_t xx = 0; xx < rt.w; xx += cw)
            {
                uint16_t fixedw = std::min(rt.w - xx, cw);
                uint16_t fixedh = std::min(rt.h - yy, ch);
                res.emplace_back(rt.x + xx, rt.y + yy, fixedw, fixedh);
            }
        }

        return res;
    }

    std::list<Region> Region::divideCounts(const Region & rt, uint16_t cols, uint16_t rows)
    {
        if(cols == 0 || rows == 0)
            throw std::runtime_error("Region::divideCounts: size empty");

        uint16_t bw = rt.w <= cols ? 1 : rt.w / cols;
        uint16_t bh = rt.h <= rows ? 1 : rt.h / rows;
        return divideBlocks(rt, Size(bw, bh));
    }

    Region Region::intersected(const Region & reg) const
    {
        Region res;
        intersection(*this, reg, & res);
        return res;
    }
        
    bool Region::intersects(const Region & reg1, const Region & reg2)
    {
        // check reg1.empty || reg2.empty
        if(reg1.isEmpty() || reg2.isEmpty())
            return false;

        // horizontal intersection
        if(std::min(reg1.x + reg1.w, reg2.x + reg2.w) <= std::max(reg1.x, reg2.x))
            return false;

        // vertical intersection
        if(std::min(reg1.y + reg1.h, reg2.y + reg2.h) <= std::max(reg1.y, reg2.y))
            return false;

        return true;
    }

    bool Region::intersection(const Region & reg1, const Region & reg2, Region* res)
    {
        bool intersects = Region::intersects(reg1, reg2);

        if(! intersects)
            return false;

        if(! res)
            return intersects;

        // horizontal intersection
        res->x = std::max(reg1.x, reg2.x);
        res->w = std::min(reg1.x + reg1.w, reg2.x + reg2.w) - res->x;

        // vertical intersection
        res->y = std::max(reg1.y, reg2.y);
        res->h = std::min(reg1.y + reg1.h, reg2.y + reg2.h) - res->y;

        return ! res->isEmpty();
    }

    /* PointerIterator */
    PointIterator & PointIterator::operator++(void)
    {
        if(! isValid())
            return *this;

        if(x < limit.w)
            x++;

        if(x == limit.w && y < limit.h)
        {
            x = 0;
            y++;

            if(y == limit.h)
            {
                x = -1;
                y = -1;
            }
        }

        return *this;
    }

    PointIterator & PointIterator::operator--(void)
    {
        if(! isValid())
            return *this;

        if(x > 0)
            x--;

        if(x == 0 && y > 0)
        {
            x = limit.w - 1;
            y--;

            if(y == 0)
            {
                x = -1;
                y = -1;
            }
        }

        return *this;
    }

    /* PixelFormat */
    PixelFormat::PixelFormat(int bpp, int dep, bool be, bool trucol, int rmax, int gmax, int bmax, int rshift, int gshift, int bshift)
        : bitsPerPixel(bpp), depth(dep), flags(0), redShift(rshift), greenShift(gshift), blueShift(bshift), redMax(rmax), greenMax(gmax), blueMax(bmax)
    {
        if(be) flags |= BigEndian;

        if(trucol) flags |= TrueColor;
    }

    PixelFormat::PixelFormat(int bpp, int dep, bool be, bool trucol, int rmask, int gmask, int bmask)
        : bitsPerPixel(bpp), depth(dep), flags(0), redShift(0), greenShift(0), blueShift(0), redMax(0), greenMax(0), blueMax(0)
    {
        if(be) flags |= BigEndian;

        if(trucol) flags |= TrueColor;

        redMax = Tools::maskMaxValue(rmask);
        greenMax = Tools::maskMaxValue(gmask);
        blueMax = Tools::maskMaxValue(bmask);
        redShift = Tools::maskShifted(rmask);
        greenShift = Tools::maskShifted(gmask);
        blueShift = Tools::maskShifted(bmask);
    }

    bool PixelFormat::operator!= (const PixelFormat & pf) const
    {
        return trueColor() != pf.trueColor() || bitsPerPixel != pf.bitsPerPixel ||
               redMax != pf.redMax || greenMax != pf.greenMax || blueMax != pf.blueMax ||
               redShift != pf.redShift || greenShift != pf.greenShift || blueShift != pf.blueShift;
    }

    uint32_t PixelFormat::rmask(void) const
    {
        return static_cast<uint32_t>(redMax) << redShift;
    }

    uint32_t PixelFormat::gmask(void) const
    {
        return static_cast<uint32_t>(greenMax) << greenShift;
    }

    uint32_t PixelFormat::bmask(void) const
    {
        return static_cast<uint32_t>(blueMax) << blueShift;
    }
        
    bool PixelFormat::bigEndian(void) const
    {
        return flags & BigEndian;
    }
            
    bool PixelFormat::trueColor(void) const
    {
        return flags & TrueColor;
    }
        
    uint32_t PixelFormat::red(int pixel) const
    {
        return (pixel >> redShift) & redMax;
    }
    
    uint32_t PixelFormat::green(int pixel) const
    {
        return (pixel >> greenShift) & greenMax;
    }

    uint32_t PixelFormat::blue(int pixel) const
    {
        return (pixel >> blueShift) & blueMax;
    }

    uint32_t PixelFormat::bytePerPixel(void) const
    {
        return bitsPerPixel >> 3;
    }

    Color PixelFormat::color(int pixel) const
    {
        return Color(red(pixel), green(pixel), blue(pixel));
    }

    uint32_t PixelFormat::pixel(const Color & col) const
    {
        return ((static_cast<int>(col.r) * redMax / 0xFF) << redShift) |
               ((static_cast<int>(col.g) * greenMax / 0xFF) << greenShift) | ((static_cast<int>(col.b) * blueMax / 0xFF) << blueShift);
    }

    uint32_t PixelFormat::convertFrom(const PixelFormat & pf, uint32_t pixel) const
    {
        if(pf != *this)
        {
            uint32_t r = (pf.red(pixel) * redMax) / pf.redMax;
            uint32_t g = (pf.green(pixel) * greenMax) / pf.greenMax;
            uint32_t b = (pf.blue(pixel) * blueMax) / pf.blueMax;
            return (r << redShift) | (g << greenShift) | (b << blueShift);
        }
            
        return pixel;
    }

    fbinfo_t::fbinfo_t(const Size & fbsz, const PixelFormat & fmt)
        : allocated(true), pitch(0), buffer(nullptr), format(fmt)
    {
        pitch = fmt.bytePerPixel() * fbsz.w;
        size_t length = pitch * fbsz.h;
        buffer = new uint8_t[length];
        std::fill(buffer, buffer + length, 0);
    }

    fbinfo_t::fbinfo_t(uint8_t* ptr, const Size & fbsz, const PixelFormat & fmt)
        : allocated(false), pitch(0), buffer(ptr), format(fmt)
    {
        pitch = fmt.bytePerPixel() * fbsz.w;
    }

    fbinfo_t::~fbinfo_t()
    {
        if(allocated)
            delete [] buffer;
    }

    int PixelMapWeight::maxWeightPixel(void) const
    {
        auto it = std::max_element(begin(), end(), [](auto & p1, auto & p2)
        {
            return p1.second < p2.second;
        });
        return it != end() ? (*it).first : 0;
    }

    /* FrameBuffer */
    FrameBuffer::FrameBuffer(const Region & reg, const FrameBuffer & fb)
        : fbptr(fb.fbptr), fbreg(reg.toPoint() + fb.fbreg.toPoint(), reg.toSize()), owner(false)
    {
    }

    FrameBuffer::FrameBuffer(const Size & rsz, const PixelFormat & fmt, size_t pitch)
        : fbptr(std::make_shared<fbinfo_t>(rsz, fmt)), fbreg(Point(0, 0), rsz), owner(true)
    {
        // fixed pitch
        if(pitch && fbptr->pitch != pitch)
            fbptr->pitch = pitch;
    }

    FrameBuffer::FrameBuffer(uint8_t* p, const Region & reg, const PixelFormat & fmt, size_t pitch)
        : fbptr(std::make_shared<fbinfo_t>(p, reg.toSize(), fmt)), fbreg(reg), owner(true)
    {
        // fixed pitch
        if(pitch && fbptr->pitch != pitch)
            fbptr->pitch = pitch;
    }

    const PixelFormat & FrameBuffer::pixelFormat(void) const
    {
        return fbptr->format;
    }

    void FrameBuffer::setPixelRow(const Point & pos, uint32_t pixel, size_t length)
    {
        if(0 <= pos.x && 0 <= pos.y && pos.x < fbreg.w && pos.y < fbreg.h)
        {
            void* offset = pitchData(pos.y) + (pos.x * bytePerPixel());
            length = std::min(length, static_cast<size_t>(fbreg.w - pos.x));
            auto bpp = bitsPerPixel();

            switch(bpp)
            {
                case 32:
                    if(auto ptr = static_cast<uint32_t*>(offset))
                        std::fill(ptr, ptr + length, pixel);
                    break;

                case 24:
                    if(auto ptr = static_cast<uint8_t*>(offset))
                    {
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
                        uint8_t v1 = pixel;
                        uint8_t v2 = pixel >> 8;
                        uint8_t v3 = pixel >> 16;
#else
                        uint8_t v1 = pixel >> 16;
                        uint8_t v2 = pixel >> 8;
                        uint8_t v3 = pixel;
#endif

                        while(length--)
                        {
                            *ptr++ = v1;
                            *ptr++ = v2;
                            *ptr++ = v3;
                        }
                    }
                    break;

                case 16:
                    if(auto ptr = static_cast<uint16_t*>(offset))
                        std::fill(ptr, ptr + length, static_cast<uint16_t>(pixel));
                    break;

                case 8:
                    if(auto ptr = static_cast<uint8_t*>(offset))
                        std::fill(ptr, ptr + length, static_cast<uint8_t>(pixel));
                    break;

                default:
                    throw std::runtime_error(std::string("FrameBuffer::setPixelRow: unknown bpp: ").append(std::to_string(bpp)));
            }
        }
    }

    void FrameBuffer::setPixel(const Point & pos, uint32_t pixel, const PixelFormat* fmt)
    {
        auto raw = fmt ? pixelFormat().convertFrom(*fmt, pixel) : pixel;
        setPixelRow(pos, raw, 1);
    }

    void FrameBuffer::setColor(const Point & pos, const Color & col)
    {
        setPixelRow(pos, pixelFormat().pixel(col), 1);
    }

    void FrameBuffer::fillPixel(const Region & reg0, uint32_t pixel, const PixelFormat* fmt)
    {
        Region reg;
        if(Region::intersection(region(), reg0, & reg))
        {
            auto raw = fmt ? pixelFormat().convertFrom(*fmt, pixel) : pixel;

            for(int yy = 0; yy < reg.h; ++yy)
                setPixelRow(reg.topLeft() + Point(0, yy), raw, reg.w);
        }
    }

    void FrameBuffer::fillColor(const Region & reg0, const Color & col)
    {
        Region reg;
        if(Region::intersection(region(), reg0, & reg))
        {
            auto raw = pixelFormat().pixel(col);

            for(int yy = 0; yy < reg.h; ++yy)
                setPixelRow(reg.topLeft() + Point(0, yy), raw, reg.w);
        }
    }

    void FrameBuffer::drawRect(const Region & reg0, const Color & col)
    {
        Region reg;
        if(Region::intersection(region(), reg0, & reg))
        {
            auto raw = pixelFormat().pixel(col);
            setPixelRow(reg.topLeft(), raw, reg.w);
            setPixelRow(reg.topLeft() + Point(0, reg.h - 1), raw, reg.w);

            for(int yy = 1; yy < reg.h - 1; ++yy)
            {
                setPixelRow(reg.topLeft() + Point(0, yy), raw, 1);
                setPixelRow(reg.topLeft() + Point(reg.w - 1, yy), raw, 1);
            }
        }
    }

    uint32_t FrameBuffer::pixel(const Point & pos) const
    {
        if(0 <= pos.x && 0 <= pos.y && pos.x < fbreg.w && pos.y < fbreg.h)
        {
            void* ptr = pitchData(pos.y) + (pos.x * bytePerPixel());
            auto bpp = bitsPerPixel();

            switch(bpp)
            {
                case 32:
                    return *static_cast<uint32_t*>(ptr);

                case 24:
                {
                    auto buf = static_cast<uint8_t*>(ptr);
                    uint32_t res = 0;
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
                    res |= buf[2]; res <<= 8; res |= buf[1]; res <<= 8; res |= buf[0];
#else
                    res |= buf[0]; res <<= 8; res |= buf[1]; res <<= 8; res |= buf[2];
#endif
                    return res;
                }

                case 16:
                    return *static_cast<uint16_t*>(ptr);

                case 8:
                    return *static_cast<uint8_t*>(ptr);

                default:
                    throw std::runtime_error(std::string("FrameBuffer::pixel: unknown bpp: ").append(std::to_string(bpp)));
            }
        }

        return 0;
    }

    std::list<PixelLength> FrameBuffer::toRLE(const Region & reg) const
    {
        std::list<PixelLength> res;

        for(auto coord = PointIterator(0, 0, reg.toSize()); coord.isValid(); ++coord)
        {
            auto pix = pixel(reg.topLeft() + coord);

            if(0 < coord.x && res.back().pixel() == pix)
                res.back().second++;
            else
                res.emplace_back(pix, 1);
        }

        return res;
    }

    void FrameBuffer::blitRegion(const FrameBuffer & fb, const Region & reg, const Point & pos)
    {
        auto dst = Region(pos, reg.toSize()).intersected(region());

        if(pixelFormat() != fb.pixelFormat())
        {
            for(auto coord = PointIterator(0, 0, dst.toSize()); coord.isValid(); ++coord)
                setPixel(dst + coord, fb.pixel(reg.topLeft() + coord), & fb.pixelFormat());
        }
        else
        {
            for(int row = 0; row < dst.h; ++row)
            {
                auto ptr = fb.pitchData(reg.y + row) + reg.x * fb.bytePerPixel();
                size_t length = dst.w * fb.bytePerPixel();
                std::copy(ptr, ptr + length, pitchData(dst.y + row) + dst.x * bytePerPixel());
            }
        }
    }

    PixelMapWeight FrameBuffer::pixelMapWeight(const Region & reg) const
    {
        PixelMapWeight map;

        for(auto coord = PointIterator(0, 0, reg.toSize()); coord.isValid(); ++coord)
        {
            auto pix = pixel(reg.topLeft() + coord);
            auto it = map.find(pix);

            if(it != map.end())
                (*it).second += 1;
            else
                map.emplace(pix, 1);
        }

        return map;
    }

    bool FrameBuffer::allOfPixel(uint32_t pixel, const Region & reg) const
    {
        for(auto coord = PointIterator(0, 0, reg.toSize()); coord.isValid(); ++coord)
            if(pixel != this->pixel(reg.topLeft() + coord)) return false;

        return true;
    }

    Color FrameBuffer::color(const Point & pos) const
    {
        return pixelFormat().color(pixel(pos));
    }

    uint32_t FrameBuffer::bitsPerPixel(void) const
    {
        return pixelFormat().bitsPerPixel;
    }

    uint32_t FrameBuffer::bytePerPixel(void) const
    {
        return pixelFormat().bytePerPixel();
    }

    size_t FrameBuffer::pitchSize(void) const
    {
        return owner ? fbptr->pitch : bytePerPixel() * fbreg.w;
    }

    uint8_t* FrameBuffer::pitchData(size_t row) const
    {
        uint32_t col = 0;
        if(! owner)
        {
            col = bytePerPixel() * fbreg.x;
            row = fbreg.y + row;
        }
        return fbptr->buffer + (fbptr->pitch * row) + col;
    }

    size_t FrameBuffer::width(void) const
    {
        return fbreg.w;
    }

    size_t FrameBuffer::height(void) const
    {
        return fbreg.h;
    }
}
