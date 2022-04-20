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

#ifndef _STORAGE_VNC_CONNECTOR_
#define _STORAGE_VNC_CONNECTOR_

#include <list>
#include <mutex>
#include <memory>
#include <future>
#include <atomic>
#include <functional>

#include "libvnc.h"
#include "network_stream.h"

namespace RFB
{
    typedef std::function<void(const FrameBuffer &)> sendEncodingFunc;

    /* Connector::VNC */
    class ServerConnector : protected Network::BaseStream
    {
        std::unique_ptr<Network::BaseStream> socket;        /// socket layer
        std::unique_ptr<Network::DeflateStream> zlib;       /// zlib layer

        Network::BaseStream* streamIn;
        Network::BaseStream* streamOut;

        int                 debug;
        int                 encodingDebug;
        int                 encodingThreads;
        std::atomic<bool>   loopMessage;
        std::atomic<bool>   fbUpdateProcessing;
        std::atomic<bool>   clientUpdateReq;
        PixelFormat         clientFormat;
        Region              clientRegion;
        std::mutex          sendGlobal;
        std::mutex          sendEncoding;
        std::vector<int>    clientEncodings;
        std::list< std::future<void> > jobsEncodings;
        std::pair<sendEncodingFunc, int> prefEncodings;

        SWE::Surface       fbSurf;
        std::unique_ptr<FrameBuffer> fbPtr;
        const SWE::JsonObject* config;

        // network stream interface
        void            sendFlush(void) override;
        void            sendRaw(const void* ptr, size_t len) override;
        void            recvRaw(void* ptr, size_t len) const override;
        bool            hasInput(void) const override;

        // zlib wrapper
        void            zlibDeflateStart(size_t);
        int             zlibDeflateStop(bool uint16sz = false);

    protected:
        bool            clientAuthVnc(bool);
        void            clientSetPixelFormat(void);
        bool            clientSetEncodings(void);
        bool            clientFramebufferUpdate(void);
        void            clientKeyEvent(void);
        void            clientPointerEvent(void);
        void            clientCutTextEvent(void);
        void            clientDisconnectedEvent(void);
        void            clientEnableContinuousUpdates(void);

        bool            serverSendFrameBufferUpdate(const Region &);
        void            serverSendBell(void);
        void            serverSendEndContinuousUpdates(void);

        int             sendPixel(uint32_t pixel);
        int             sendCPixel(uint32_t pixel);
        int             sendRunLength(size_t length);

        bool            isUpdateProcessed(void) const;
        void            waitSendingFBUpdate(void) const;

        void            sendEncodingRaw(const FrameBuffer &);
        void            sendEncodingRawSubRegion(const Point &, const Region &, const FrameBuffer &, int jobId);
        void            sendEncodingRawSubRegionRaw(const Region &, const FrameBuffer &);

        void            sendEncodingRRE(const FrameBuffer &, bool corre);
        void            sendEncodingRRESubRegion(const Point &, const Region &, const FrameBuffer &, int jobId, bool corre);
        void            sendEncodingRRESubRects(const Region &, const FrameBuffer &, int jobId, int back, const std::list<RegionPixel> &, bool corre);

        void            sendEncodingHextile(const FrameBuffer &, bool zlibver);
        void            sendEncodingHextileSubRegion(const Point &, const Region &, const FrameBuffer &, int jobId, bool zlibver);
        void            sendEncodingHextileSubForeground(const Region &, const FrameBuffer &, int jobId, int back, const std::list<RegionPixel> &);
        void            sendEncodingHextileSubColored(const Region &, const FrameBuffer &, int jobId, int back, const std::list<RegionPixel> &);
        void            sendEncodingHextileSubRaw(const Region &, const FrameBuffer &, int jobId, bool zlibver);

        void            sendEncodingZLib(const FrameBuffer &);
        void            sendEncodingZLibSubRegion(const Point &, const Region &, const FrameBuffer &, int jobId);

        void            sendEncodingTRLE(const FrameBuffer &, bool zrle);
        void            sendEncodingTRLESubRegion(const Point &, const Region &, const FrameBuffer &, int jobId, bool zrle);
        void            sendEncodingTRLESubPacked(const Region &, const FrameBuffer &, int jobId, size_t field, const PixelMapWeight &, bool zrle);
        void            sendEncodingTRLESubPlain(const Region &, const FrameBuffer &, const std::list<PixelLength> &);
        void            sendEncodingTRLESubPalette(const Region &, const FrameBuffer &, const PixelMapWeight &, const std::list<PixelLength> &);
        void            sendEncodingTRLESubRaw(const Region &, const FrameBuffer &);

        std::pair<sendEncodingFunc, int> selectEncodings(void);

    public:
        ServerConnector(TCPsocket sock, const SWE::JsonObject* jo);
        ~ServerConnector();

        int             communication(const std::string &);
        void            shutdown(void);
        void            setFrameBuffer(const SWE::Surface &);
    };
}

#endif
