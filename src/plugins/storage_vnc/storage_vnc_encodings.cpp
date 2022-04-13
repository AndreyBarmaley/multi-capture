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

#include <cmath>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <future>
#include <iterator>

#include "storage_vnc_connector.h"

using namespace std::chrono_literals;

namespace RFB
{
    std::pair<sendEncodingFunc, int> ServerConnector::selectEncodings(void)
    {
        for(int type : clientEncodings)
        {
            switch(type)
            {
                case RFB::ENCODING_ZLIB:
                    return std::make_pair([ = ](const FrameBuffer & fb)
                    {
                        return this->sendEncodingZLib(fb);
                    }, type);

                case RFB::ENCODING_HEXTILE:
                    return std::make_pair([ = ](const FrameBuffer & fb)
                    {
                        return this->sendEncodingHextile(fb, false);
                    }, type);

                case RFB::ENCODING_ZLIBHEX:
                    return std::make_pair([ = ](const FrameBuffer & fb)
                    {
                        return this->sendEncodingHextile(fb, true);
                    }, type);

                case RFB::ENCODING_CORRE:
                    return std::make_pair([ = ](const FrameBuffer & fb)
                    {
                        return this->sendEncodingRRE(fb, true);
                    }, type);

                case RFB::ENCODING_RRE:
                    return std::make_pair([ = ](const FrameBuffer & fb)
                    {
                        return this->sendEncodingRRE(fb, false);
                    }, type);

                case RFB::ENCODING_TRLE:
                    return std::make_pair([ = ](const FrameBuffer & fb)
                    {
                        return this->sendEncodingTRLE(fb, false);
                    }, type);

                case RFB::ENCODING_ZRLE:
                    return std::make_pair([ = ](const FrameBuffer & fb)
                    {
                        return this->sendEncodingTRLE(fb, true);
                    }, type);

                default:
                    break;
            }
        }
        return std::make_pair([ = ](const FrameBuffer & fb)
        {
            return this->sendEncodingRaw(fb);
        }, RFB::ENCODING_RAW);
    }

    void ServerConnector::sendEncodingRaw(const FrameBuffer & fb)
    {
        const Region & reg0 = fb.region();
        if(1 < debug)
          DEBUG("encoding: Raw, region: [" << reg0.x << ", " << reg0.y << ", " << reg0.w << ", " << reg0.h << "]");
        // regions counts
        sendIntBE16(1);
        sendEncodingRawSubRegion(Point(0, 0), reg0, fb, 1);
    }

    void ServerConnector::sendEncodingRawSubRegion(const Point & top, const Region & reg, const FrameBuffer & fb, int jobId)
    {
        const std::lock_guard<std::mutex> lock(sendEncoding);

        if(3 < debug)
        {
            DEBUG("send RAW region, job id: " << jobId << ", [" << reg.x << ", " << reg.y << ", " << reg.w << ", " << reg.h << "]");
        }

        // region size
        sendIntBE16(top.x + reg.x);
        sendIntBE16(top.y + reg.y);
        sendIntBE16(reg.w);
        sendIntBE16(reg.h);
        // region type
        sendIntBE32(RFB::ENCODING_RAW);
        sendEncodingRawSubRegionRaw(reg, fb);
    }

    void ServerConnector::sendEncodingRawSubRegionRaw(const Region & reg, const FrameBuffer & fb)
    {
        if(fb.pixelFormat() != clientFormat)
        {
            for(auto coord = PointIterator(0, 0, reg.toSize()); coord.isValid(); ++coord)
                sendPixel(fb.pixel(reg.topLeft() + coord));
        }
        else
        {
            for(int yy = 0; yy < reg.h; ++yy)
            {
                sendRaw(fb.pitchData(reg.y + yy) + reg.x * fb.bytePerPixel(), fb.pitchSize());
            }
        }
    }

    std::list<RegionPixel> processingRRE(const Region & badreg, const FrameBuffer & fb, int skipPixel)
    {
        std::list<RegionPixel> goods;
        std::list<Region> bads1 = { badreg };
        std::list<Region> bads2;

        do
        {
            while(! bads1.empty())
            {
                for(auto & subreg : Region::divideCounts(bads1.front(), 2, 2))
                {
                    auto pixel = fb.pixel(subreg.topLeft());

                    if((subreg.w == 1 && subreg.h == 1) || fb.allOfPixel(pixel, subreg))
                    {
                        if(pixel != skipPixel)
                        {
                            // maybe join prev
                            if(! goods.empty() && goods.back().first.y == subreg.y && goods.back().first.h == subreg.h &&
                               goods.back().first.x + goods.back().first.w == subreg.x && goods.back().second == pixel)
                                goods.back().first.w += subreg.w;
                            else
                                goods.emplace_back(subreg, pixel);
                        }
                    }
                    else
                        bads2.push_back(subreg);
                }

                bads1.pop_front();
            }

            if(bads2.empty())
                break;

            bads2.swap(bads1);
            bads2.clear();
        }
        while(! bads1.empty());

        return goods;
    }

    /* RRE */
    void ServerConnector::sendEncodingRRE(const FrameBuffer & fb, bool corre)
    {
        const Region & reg0 = fb.region();
        if(1 < debug)
          DEBUG("encoding: " << (corre ? "CoRRE" : "RRE") << ", region: [" << reg0.x << ", " << reg0.y << ", " << reg0.w << ", " << reg0.h << "]");
        const Point top(reg0.x, reg0.y);
        const Size bsz = corre ? Size(64, 64) : Size(128, 128);
        auto regions = Region::divideBlocks(reg0, bsz);
        // regions counts
        sendIntBE16(regions.size());
        int jobId = 1;

        // make pool jobs
        while(jobId <= encodingThreads && ! regions.empty())
        {
            jobsEncodings.push_back(std::async(std::launch::async, & ServerConnector::sendEncodingRRESubRegion, this, top, regions.front() - top, fb, jobId, corre));
            regions.pop_front();
            jobId++;
        }

        // renew completed job
        while(! regions.empty())
        {
            for(auto & job : jobsEncodings)
            {
                if(regions.empty())
                    break;

                if(job.wait_for(std::chrono::microseconds(1)) == std::future_status::ready)
                {
                    job = std::async(std::launch::async, & ServerConnector::sendEncodingRRESubRegion, this, top, regions.front() - top, fb, jobId, corre);
                    regions.pop_front();
                    jobId++;
                }
            }
        }

        // wait jobs
        for(auto & job : jobsEncodings)
            job.wait();

        jobsEncodings.clear();
    }

    void ServerConnector::sendEncodingRRESubRegion(const Point & top, const Region & reg, const FrameBuffer & fb, int jobId, bool corre)
    {
        auto map = fb.pixelMapWeight(reg);
        auto sendHeaderRRE = [this](const Region & reg, bool corre)
        {
            // region size
            this->sendIntBE16(reg.x);
            this->sendIntBE16(reg.y);
            this->sendIntBE16(reg.w);
            this->sendIntBE16(reg.h);
            // region type
            this->sendIntBE32(corre ? RFB::ENCODING_CORRE : RFB::ENCODING_RRE);
        };

        if(map.empty())
            throw std::runtime_error("ServerConnector::sendEncodingRRESubRegion: pixel map is empty");

        if(map.size() > 1)
        {
            int back = map.maxWeightPixel();
            std::list<RegionPixel> goods = processingRRE(reg, fb, back);
            const size_t rawLength = reg.h * fb.pitchSize();
            const size_t rreLength = 4 + fb.bytePerPixel() + goods.size() * (fb.bytePerPixel() + (corre ? 4 : 8));

            // compare with raw
            if(rawLength < rreLength)
            {
                sendEncodingRawSubRegion(top, reg, fb, jobId);
            }
            else
            {
                const std::lock_guard<std::mutex> lock(sendEncoding);

                if(3 < debug)
                {
                    DEBUG("send " << (corre ? "CoRRE" : "RRE") << " region, job id: " << jobId <<
                      ", [" << top.x + reg.x << ", " << top.y + reg.y << ", " << reg.w << ", " << reg.h << "], back pixel " <<
                      SWE::String::hex(back) << ", sub rects: " << goods.size());
                }

                sendHeaderRRE(reg + top, corre);
                sendEncodingRRESubRects(reg, fb, jobId, back, goods, corre);
            }
        }
        // if(map.size() == 1)
        else
        {
            int back = fb.pixel(reg.topLeft());
            const std::lock_guard<std::mutex> lock(sendEncoding);

            if(3 < debug)
            {
                DEBUG("send " << (corre ? "CoRRE" : "RRE") << " region, job id: " <<
                    jobId << ", [" << top.x + reg.x << ", " << top.y + reg.y << ", " << reg.w << ", " << reg.h <<
                    " ], back pixel " << SWE::String::hex(back) << ", solid");
            }

            sendHeaderRRE(reg + top, corre);
            // num sub rects
            sendIntBE32(1);
            // back pixel
            sendPixel(back);
            /* one fake sub region : RRE requires */
            // subrect pixel
            sendPixel(back);

            // subrect region (relative coords)
            if(corre)
            {
                sendInt8(0);
                sendInt8(0);
                sendInt8(1);
                sendInt8(1);
            }
            else
            {
                sendIntBE16(0);
                sendIntBE16(0);
                sendIntBE16(1);
                sendIntBE16(1);
            }
        }
    }

    void ServerConnector::sendEncodingRRESubRects(const Region & reg, const FrameBuffer & fb, int jobId, int back, const std::list<RegionPixel> & rreList, bool corre)
    {
        // num sub rects
        sendIntBE32(rreList.size());
        // back pixel
        sendPixel(back);

        for(auto & pair : rreList)
        {
            // subrect pixel
            sendPixel(pair.pixel());
            auto & region = pair.region();

            // subrect region (relative coords)
            if(corre)
            {
                sendInt8(region.x - reg.x);
                sendInt8(region.y - reg.y);
                sendInt8(region.w);
                sendInt8(region.h);
            }
            else
            {
                sendIntBE16(region.x - reg.x);
                sendIntBE16(region.y - reg.y);
                sendIntBE16(region.w);
                sendIntBE16(region.h);
            }

            if(4 < debug)
            {
                DEBUG("send " << (corre ? "CoRRE" : "RRE") << " sub region, job id: " <<
                    jobId << ", [" << region.x - reg.x << ", " << region.y - reg.y << ", " << region.w << ", " << region.h <<
                    "], back pixel " << SWE::String::hex(pair.pixel()));
            }
        }
    }

    /* HexTile */
    void ServerConnector::sendEncodingHextile(const FrameBuffer & fb, bool zlibver)
    {
        const Region & reg0 = fb.region();
        if(1 < debug)
          DEBUG("encoding: HexTile, region: [" << reg0.x << ", " << reg0.y << ", " << reg0.w << ", " << reg0.h << "]");
        const Point top(reg0.x, reg0.y);
        const Size bsz = Size(16, 16);
        auto regions = Region::divideBlocks(reg0, bsz);
        // regions counts
        sendIntBE16(regions.size());
        int jobId = 1;

        // make pool jobs
        while(jobId <= encodingThreads && ! regions.empty())
        {
            jobsEncodings.push_back(std::async(std::launch::async, & ServerConnector::sendEncodingHextileSubRegion, this, top, regions.front() - top, fb, jobId, zlibver));
            regions.pop_front();
            jobId++;
        }

        // renew completed job
        while(! regions.empty())
        {
            for(auto & job : jobsEncodings)
            {
                if(regions.empty())
                    break;

                if(job.wait_for(std::chrono::microseconds(1)) == std::future_status::ready)
                {
                    job = std::async(std::launch::async, & ServerConnector::sendEncodingHextileSubRegion, this, top, regions.front() - top, fb, jobId, zlibver);
                    regions.pop_front();
                    jobId++;
                }
            }
        }

        // wait jobs
        for(auto & job : jobsEncodings)
            job.wait();

        jobsEncodings.clear();
    }

    void ServerConnector::sendEncodingHextileSubRegion(const Point & top, const Region & reg, const FrameBuffer & fb, int jobId, bool zlibver)
    {
        auto map = fb.pixelMapWeight(reg);
        auto sendHeaderHexTile = [this](const Region & reg, bool zlibver)
        {
            // region size
            this->sendIntBE16(reg.x);
            this->sendIntBE16(reg.y);
            this->sendIntBE16(reg.w);
            this->sendIntBE16(reg.h);
            // region type
            this->sendIntBE32(zlibver ? RFB::ENCODING_ZLIBHEX : RFB::ENCODING_HEXTILE);
        };

        if(map.empty())
            throw std::runtime_error("ServerConnector::sendEncodingHextileSubRegion: pixel map is empty");

        if(map.size() == 1)
        {
            // wait thread
            const std::lock_guard<std::mutex> lock(sendEncoding);
            sendHeaderHexTile(reg + top, zlibver);
            int back = fb.pixel(reg.topLeft());

            if(3 < debug)
            {
                DEBUG("send HexTile region, job id: " <<
                    jobId << ", [" << top.x + reg.x << ", " << top.y + reg.y << ", " << reg.w << ", " << reg.h <<
                    "], back pixel: " << SWE::String::hex(back) << ", solid");
            }

            // hextile flags
            sendInt8(RFB::HEXTILE_BACKGROUND);
            sendPixel(back);
        }
        else if(map.size() > 1)
        {
            // no wait, worked
            int back = map.maxWeightPixel();
            std::list<RegionPixel> goods = processingRRE(reg, fb, back);
            // all other color
            bool foreground = std::all_of(goods.begin(), goods.end(),
                              [col = goods.front().second](auto & pair) { return pair.pixel() == col; });
            const size_t hextileRawLength = 1 + reg.h * fb.pitchSize();
            // wait thread
            const std::lock_guard<std::mutex> lock(sendEncoding);
            sendHeaderHexTile(reg + top, zlibver);

            if(foreground)
            {
                const size_t hextileForegroundLength = 2 + 2 * fb.bytePerPixel() + goods.size() * 2;

                // compare with raw
                if(hextileRawLength < hextileForegroundLength)
                {
                    if(3 < debug)
                    {
                        DEBUG("send HexTile region, job id: " <<
                            jobId << ", [" << top.x + reg.x << ", " << top.y + reg.y << ", " << reg.w << ", " << reg.h << "], raw");
                    }

                    sendEncodingHextileSubRaw(reg, fb, jobId, zlibver);
                }
                else
                {
                    if(3 < debug)
                    {
                        DEBUG("send HexTile region, job id: " <<
                            jobId << ", [" << top.x + reg.x << ", " << top.y + reg.y << ", " << reg.w << ", " << reg.h <<
                            "], back pixel: " << SWE::String::hex(back) << ", sub rects: " << goods.size() << ", foreground");
                    }

                    sendEncodingHextileSubForeground(reg, fb, jobId, back, goods);
                }
            }
            else
            {
                const size_t hextileColoredLength = 2 + fb.bytePerPixel() + goods.size() * (2 + fb.bytePerPixel());

                // compare with raw
                if(hextileRawLength < hextileColoredLength)
                {
                    if(3 < debug)
                    {
                        DEBUG("send HexTile region, job id: " <<
                            jobId << ", [" << top.x + reg.x << ", " << top.y + reg.y << ", " << reg.w << ", " << reg.h << "], raw");
                    }

                    sendEncodingHextileSubRaw(reg, fb, jobId, zlibver);
                }
                else
                {
                    if(3 < debug)
                    {
                        DEBUG("send HexTile region, job id: " <<
                            jobId << ", [" << top.x + reg.x << ", " << top.y + reg.y << ", " << reg.w << ", " << reg.h <<
                            "], back pixel: " << SWE::String::hex(back) << ", sub rects: " << goods.size() << ", colored");
                    }

                    sendEncodingHextileSubColored(reg, fb, jobId, back, goods);
                }
            }
        }
    }

    void ServerConnector::sendEncodingHextileSubColored(const Region & reg, const FrameBuffer & fb, int jobId, int back, const std::list<RegionPixel> & rreList)
    {
        // hextile flags
        sendInt8(RFB::HEXTILE_BACKGROUND | RFB::HEXTILE_COLOURED | RFB::HEXTILE_SUBRECTS);
        // hextile background
        sendPixel(back);
        // hextile subrects
        sendInt8(rreList.size());

        for(auto & pair : rreList)
        {
            auto & region = pair.region();
            sendPixel(pair.pixel());
            sendInt8(0xFF & ((region.x - reg.x) << 4 | (region.y - reg.y)));
            sendInt8(0xFF & ((region.w - 1) << 4 | (region.h - 1)));

            if(4 < debug)
            {
                DEBUG("send HexTile sub region, job id: " << 
                    jobId << ", [" << region.x - reg.x << ", " << region.y - reg.y << ", " << region.w << ", " << region.h <<
                    "], back pixel: " << SWE::String::hex(pair.pixel()));
            }
        }
    }

    void ServerConnector::sendEncodingHextileSubForeground(const Region & reg, const FrameBuffer & fb, int jobId, int back, const std::list<RegionPixel> & rreList)
    {
        // hextile flags
        sendInt8(RFB::HEXTILE_BACKGROUND | RFB::HEXTILE_FOREGROUND | RFB::HEXTILE_SUBRECTS);
        // hextile background
        sendPixel(back);
        // hextile foreground
        sendPixel(rreList.front().second);
        // hextile subrects
        sendInt8(rreList.size());

        for(auto & pair : rreList)
        {
            auto & region = pair.region();
            sendInt8(0xFF & ((region.x - reg.x) << 4 | (region.y - reg.y)));
            sendInt8(0xFF & ((region.w - 1) << 4 | (region.h - 1)));

            if(4 < debug)
            {
                DEBUG("send HexTile sub region, job id: " <<
                    jobId << ", [" << region.x - reg.x << ", " << region.y - reg.y << ", " << region.w << ", " << region.h << "]");
            }
        }
    }

    void ServerConnector::sendEncodingHextileSubRaw(const Region & reg, const FrameBuffer & fb, int jobId, bool zlibver)
    {
        if(zlibver)
        {
            // hextile flags
            sendInt8(RFB::HEXTILE_ZLIBRAW);
            zlibDeflateStart(reg.h * fb.pitchSize());
            sendEncodingRawSubRegionRaw(reg, fb);
            zlibDeflateStop(true);
        }
        else
        {
            // hextile flags
            sendInt8(RFB::HEXTILE_RAW);
            sendEncodingRawSubRegionRaw(reg, fb);
        }
    }

    /* ZLib */
    void ServerConnector::sendEncodingZLib(const FrameBuffer & fb)
    {
        const Region & reg0 = fb.region();
        if(1 < debug)
          DEBUG("encoding: ZLib, region: [" << reg0.x << ", " << reg0.y << ", " << reg0.w << ", " << reg0.h << "]");
        // zlib specific: single thread only
        sendIntBE16(1);
        sendEncodingZLibSubRegion(Point(0, 0), reg0, fb, 1);
    }

    void ServerConnector::sendEncodingZLibSubRegion(const Point & top, const Region & reg, const FrameBuffer & fb, int jobId)
    {
        const std::lock_guard<std::mutex> lock(sendEncoding);

        if(3 < debug)
        {
            DEBUG("send ZLib region, job id: " << 
                jobId << ", [" << top.x + reg.x << ", " << top.y + reg.y << ", " << reg.w << ", " << reg.h << "]");
        }
    
        // region size
        sendIntBE16(top.x + reg.x);
        sendIntBE16(top.y + reg.y);
        sendIntBE16(reg.w);
        sendIntBE16(reg.h);
        // region type
        sendIntBE32(RFB::ENCODING_ZLIB);
        zlibDeflateStart(reg.h * fb.pitchSize());
        sendEncodingRawSubRegionRaw(reg, fb);
        zlibDeflateStop();
    }

    /* TRLE */
    void ServerConnector::sendEncodingTRLE(const FrameBuffer & fb, bool zrle)
    {
        const Region & reg0 = fb.region();
        if(1 < debug)
            DEBUG("encoding: " << (zrle ? "ZRLE" : "TRLE") << ", region: [ " << reg0.x << ", " << reg0.y << ", " << reg0.w << ", " << reg0.h << "]");
        const Point top(reg0.x, reg0.y);
        const Size bsz = Size(64, 64);
        auto regions = Region::divideBlocks(reg0, bsz);
        // regions counts
        sendIntBE16(regions.size());
        int jobId = 1;

        // make pool jobs
        while(jobId <= encodingThreads && ! regions.empty())
        {
            jobsEncodings.push_back(std::async(std::launch::async, & ServerConnector::sendEncodingTRLESubRegion, this, top, regions.front() - top, fb, jobId, zrle));
            regions.pop_front();
            jobId++;
        }

        // renew completed job
        while(! regions.empty())
        {
            for(auto & job : jobsEncodings)
            {
                if(regions.empty())
                    break;

                if(job.wait_for(std::chrono::microseconds(1)) == std::future_status::ready)
                {
                    job = std::async(std::launch::async, & ServerConnector::sendEncodingTRLESubRegion, this, top, regions.front() - top, fb, jobId, zrle);
                    regions.pop_front();
                    jobId++;
                }
            }
        }

        // wait jobs
        for(auto & job : jobsEncodings)
            job.wait();

        jobsEncodings.clear();
    }

    void ServerConnector::sendEncodingTRLESubRegion(const Point & top, const Region & reg, const FrameBuffer & fb, int jobId, bool zrle)
    {
        auto map = fb.pixelMapWeight(reg);
        // convert to palette
        int index = 0;

        for(auto & pair : map)
            pair.second = index++;

        auto sendHeaderTRLE = [this](const Region & reg, bool zrle)
        {
            // region size
            this->sendIntBE16(reg.x);
            this->sendIntBE16(reg.y);
            this->sendIntBE16(reg.w);
            this->sendIntBE16(reg.h);
            // region type
            this->sendIntBE32(zrle ? RFB::ENCODING_ZRLE : RFB::ENCODING_TRLE);
        };

        // wait thread
        const std::lock_guard<std::mutex> lock(sendEncoding);
        sendHeaderTRLE(reg + top, zrle);

        if(zrle)
            zlibDeflateStart(reg.h * fb.pitchSize());

        if(map.size() == 1)
        {
            int back = fb.pixel(reg.topLeft());

            if(3 < debug)
            {
                DEBUG("send " << (zrle ? "ZRLE" : "TRLE") << " region, job id: " <<
                        jobId << ", [" << top.x + reg.x << ", " << top.y + reg.y << ", " << reg.w << ", " << reg.h <<
                        "], back pixel: " << SWE::String::hex(back) << ", solid");
            }

            // subencoding type: solid tile
            sendInt8(1);
            sendCPixel(back);
        }
        else if(2 <= map.size() && map.size() <= 16)
        {
            size_t field = 1;

            if(4 < map.size())
                field = 4;
            else if(2 < map.size())
                field = 2;

            if(3 < debug)
            {
                DEBUG("send " << (zrle ? "ZRLE" : "TRLE") << " region, job id: " <<
                    jobId << ", [" << top.x + reg.x << ", " << top.y + reg.y << ", " << reg.w << ", " << reg.h <<
                    "], palsz: " << map.size() << ", packed: %d" << field);
            }

            sendEncodingTRLESubPacked(reg, fb, jobId, field, map, zrle);
        }
        else
        {
            auto rleList = fb.FrameBuffer::toRLE(reg);
            // rle plain size
            const size_t rlePlainLength = std::accumulate(rleList.begin(), rleList.end(), 1,
                                          [](int v, auto & pair)
            {
                return v + 3 + std::floor((pair.second - 1) / 255.0) + 1;
            });
            // rle palette size (2, 127)
            const size_t rlePaletteLength = 1 < rleList.size() && rleList.size() < 128 ? std::accumulate(rleList.begin(), rleList.end(), 1 + 3 * map.size(),
                                            [](int v, auto & pair)
            {
                return v + 1 + std::floor((pair.second - 1) / 255.0) + 1;
            }) : 0xFFFF;
            // raw length
            const size_t rawLength = 1 + 3 * reg.w * reg.h;

            if(rlePlainLength < rlePaletteLength && rlePlainLength < rawLength)
            {
                if(3 < debug)
                {
                    DEBUG("send " << (zrle ? "ZRLE" : "TRLE") << " region, job id: " <<
                        jobId << ", [" << top.x + reg.x << ", " << top.y + reg.y << ", " << reg.w << ", " << reg.h <<
                        "], length: " << rleList.size() << ", rle plain");
                }

                sendEncodingTRLESubPlain(reg, fb, rleList);
            }
            else if(rlePaletteLength < rlePlainLength && rlePaletteLength < rawLength)
            {
                if(3 < debug)
                {
                    DEBUG("send " << (zrle ? "ZRLE" : "TRLE") << " region, job id: " <<
                        jobId << ", [" << top.x + reg.x << ", " << top.y + reg.y << ", " << reg.w << ", " << reg.h <<
                        "], pal size: " << map.size() << ", length: " << rleList.size() << ", rle palette");
                }

                sendEncodingTRLESubPalette(reg, fb, map, rleList);
            }
            else
            {
                if(3 < debug)
                {
                    DEBUG("send " << (zrle ? "ZRLE" : "TRLE") << " region, job id: " <<
                        jobId << ", [" << top.x + reg.x << ", " << top.y + reg.y << ", " << reg.w << ", " << reg.h << "], raw");
                }

                sendEncodingTRLESubRaw(reg, fb);
            }
        }

        if(zrle)
            zlibDeflateStop();
    }

    void ServerConnector::sendEncodingTRLESubPacked(const Region & reg, const FrameBuffer & fb, int jobId, size_t field, const PixelMapWeight & pal, bool zrle)
    {
        // subencoding type: packed palette
        sendInt8(pal.size());
        // send palette
        for(auto & pair : pal)
            sendCPixel(pair.first);

        Tools::StreamBitsPack sb;

        // send packed rows
        for(int oy = 0; oy < reg.h; ++oy)
        {
            for(int ox = 0; ox < reg.w; ++ox)
            {
                auto pixel = fb.pixel(reg.topLeft() + Point(ox, oy));
                auto it = pal.find(pixel);
                auto index = it != pal.end() ? (*it).second : 0;

                sb.pushValue(index, field);
            }

            sb.pushAlign();
        }

        sendData(sb.toVector());

        if(4 < debug)
        {
            std::string str = SWE::Tools::buffer2HexString<uint8_t>(sb.toVector().data(), sb.toVector().size(), 2);
            DEBUG("send " << (zrle ? "ZRLE" : "TRLE") << " region, job id: " << jobId << ", packed stream: " << str);
        }
    }

    void ServerConnector::sendEncodingTRLESubPlain(const Region & reg, const FrameBuffer & fb, const std::list<PixelLength> & rle)
    {
        // subencoding type: rle plain
        sendInt8(128);

        // send rle content
        for(auto & pair : rle)
        {
            sendCPixel(pair.pixel());
            sendRunLength(pair.length());
        }
    }

    void ServerConnector::sendEncodingTRLESubPalette(const Region & reg, const FrameBuffer & fb, const PixelMapWeight & pal, const std::list<PixelLength> & rle)
    {
        // subencoding type: rle palette
        sendInt8(pal.size() + 128);
        // send palette
        for(auto & pair : pal)
            sendCPixel(pair.first);

        // send rle indexes
        for(auto & pair : rle)
        {
            auto it = pal.find(pair.pixel());
            auto index = it != pal.end() ? (*it).second : 0;

            if(1 == pair.length())
            {
                sendInt8(index);
            }
            else
            {
                sendInt8(index + 128);
                sendRunLength(pair.length());
            }
        }
    }

    void ServerConnector::sendEncodingTRLESubRaw(const Region & reg, const FrameBuffer & fb)
    {
        // subencoding type: raw
        sendInt8(0);

        // send pixels
        for(auto coord = PointIterator(0, 0, reg.toSize()); coord.isValid(); ++coord)
            sendCPixel(fb.pixel(reg.topLeft() + coord));
    }
}
