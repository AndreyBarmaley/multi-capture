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

#ifndef _CAPTURE_VNC_CONNECTOR_
#define _CAPTURE_VNC_CONNECTOR_

#include <mutex>
#include <memory>
#include <atomic>

#include "libvnc.h"
#include "network_stream.h"

namespace RFB
{
    /* Connector::VNC */
    class ClientConnector : protected Network::BaseStream
    {
        std::unique_ptr<Network::BaseStream> socket;
        std::unique_ptr<Network::InflateStream> zlib;       /// zlib layer

        Network::BaseStream* streamIn;
        Network::BaseStream* streamOut;

        int             debug;
        int             sdlFormat;
        std::atomic<bool> loopMessage;
        const SWE::JsonObject* config;
        std::unique_ptr<FrameBuffer> fbPtr;
        std::mutex      fbChange;

        // network stream interface
        void            sendFlush(void) override;
        void            sendRaw(const void* ptr, size_t len) override;
        void            recvRaw(void* ptr, size_t len) const override;
        bool            hasInput(void) const override;

        // zlib wrapper
        void            zlibInflateStart(bool uint16sz = false);
        void            zlibInflateStop(void);

    protected:
        void            clientPixelFormat(void);
        void            clientSetEncodings(std::initializer_list<int>);
        void            clientFrameBufferUpdateReq(const Region &);

        void            serverFBUpdateEvent(void);
        void            serverSetColorMapEvent(void);
        void            serverBellEvent(void);
        void            serverCutTextEvent(void);

        void            recvDecodingRaw(const Region &);
        void            recvDecodingRRE(const Region &, bool corre);
        void            recvDecodingHexTile(const Region &);
        void            recvDecodingHexTileRegion(const Region &, int & bgColor, int & fgColor);
        void            recvDecodingTRLE(const Region &, bool zrle);
        void            recvDecodingTRLERegion(const Region &, bool zrle);
        void            recvDecodingZlib(const Region &);
        void            recvDecodingLastRect(const Region &);

        int             recvPixel(void);
        int             recvCPixel(void);
        size_t          recvRunLength(void);

    public:
        ClientConnector(const SWE::JsonObject*);
        ~ClientConnector(){}

        bool            communication(const std::string &, int, const std::string & pass = "");
        void            messages(void);
        void            shutdown(void);

        void            syncFrameBuffer(Surface &);
    };
}

#endif
