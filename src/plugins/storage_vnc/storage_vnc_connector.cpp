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

#include <chrono>
#include <thread>
#include <cstring>
#include <fstream>
#include <algorithm>

#include "../../settings.h"
#include "storage_vnc_connector.h"

using namespace std::chrono_literals;

namespace RFB
{
    /* Connector */
    ServerConnector::ServerConnector(TCPsocket sock, const SWE::JsonObject* jo)
        : streamIn(nullptr), streamOut(nullptr), debug(0), encodingDebug(0), encodingThreads(2),
            loopMessage(true), fbUpdateProcessing(false), clientUpdateReq(false), fbPtr(nullptr), config(jo)
    {
        debug = config->getInteger("debug", 0);
        socket.reset(new Network::TCPStream(sock));
        streamIn = streamOut = socket.get();
    }

    ServerConnector::~ServerConnector()
    {
        loopMessage = false;

        waitSendingFBUpdate();
    }

    void ServerConnector::sendFlush(void)
    {
        if(loopMessage)
            streamOut->sendFlush();
    }

    void ServerConnector::sendRaw(const void* ptr, size_t len)
    {
        if(loopMessage)
            streamOut->sendRaw(ptr, len);
    }

    void ServerConnector::recvRaw(void* ptr, size_t len) const
    {
        if(loopMessage)
            streamIn->recvRaw(ptr, len);
    }

    bool ServerConnector::hasInput(void) const
    {
        return loopMessage ? streamIn->hasInput() : false;
    }

    bool ServerConnector::clientAuthVnc(bool debug)
    {
        std::vector<uint8_t> challenge = TLS::randomKey(16);
        std::vector<uint8_t> response(16);

        if(debug)
        {
            auto tmp = SWE::Tools::buffer2HexString<uint8_t>(challenge.data(), challenge.size(), 2);
            DEBUG("challenge: " << tmp);
        }

        sendRaw(challenge.data(), challenge.size());
        sendFlush();
        recvRaw(response.data(), response.size());

        if(debug)
        {
            auto tmp = SWE::Tools::buffer2HexString<uint8_t>(response.data(), response.size(), 2);
            DEBUG("response: " << tmp);
        }

        std::string pass = config->getString("password");

        if(! pass.empty())
        {
            auto crypt = TLS::encryptDES(challenge, pass);

            if(debug)
            {
                auto tmp = SWE::Tools::buffer2HexString<uint8_t>(crypt.data(), crypt.size(), 2);
                DEBUG("encrypt: " << tmp);
            }

            if(crypt == response)
                return true;
        }

        std::ifstream ifs(config->getString("passwdfile"), std::ifstream::in);

        while(ifs.good())
        {
            std::getline(ifs, pass);
            auto crypt = TLS::encryptDES(challenge, pass);

            if(debug)
            {
                auto tmp = SWE::Tools::buffer2HexString<uint8_t>(crypt.data(), crypt.size(), 2);
                DEBUG("encrypt: " << tmp);
            }

            if(crypt == response)
                return true;
        }

        const std::string err("password mismatch");
        sendIntBE32(RFB::SECURITY_RESULT_ERR).sendIntBE32(err.size()).sendString(err).sendFlush();
        ERROR(err);
        return false;
    }

    int ServerConnector::communication(const std::string & remoteaddr)
    {
        DEBUG("remote addr: " << remoteaddr);
        encodingThreads = config->getInteger("threads", 2);

        if(encodingThreads < 1)
            encodingThreads = 1;
        else if(std::thread::hardware_concurrency() < encodingThreads)
        {
            encodingThreads = std::thread::hardware_concurrency();
            ERROR("encoding threads incorrect, fixed to hardware concurrency: " << encodingThreads);
        }

        DEBUG("using encoding threads: " << encodingThreads);
        bool noAuth = config->getBoolean("noauth", false);
        prefEncodings = selectEncodings();
        // RFB 6.1.1 version
        const std::string version = SWE::StringFormat("RFB 00%1.00%2\n").arg(RFB::VERSION_MAJOR).arg(RFB::VERSION_MINOR);
        sendString(version).sendFlush();
        std::string magick = recvString(12);
        if(debug)
            DEBUG("RFB 6.1.1, handshake version: " << magick);

        if(magick != version)
        {
            ERROR("handshake failure");
            return EXIT_FAILURE;
        }

        // RFB 6.1.2 security
        sendInt8(1);
        sendInt8(noAuth ? RFB::SECURITY_TYPE_NONE : RFB::SECURITY_TYPE_VNC);
        sendFlush();
        int clientSecurity = recvInt8();
        if(debug)
            DEBUG("RFB 6.1.2, client security: " << SWE::String::hex(clientSecurity, 2));

        if(noAuth && clientSecurity == RFB::SECURITY_TYPE_NONE)
            sendIntBE32(RFB::SECURITY_RESULT_OK).sendFlush();
        else if(clientSecurity == RFB::SECURITY_TYPE_VNC)
        {
            if(! clientAuthVnc(false))
                return EXIT_FAILURE;

            sendIntBE32(RFB::SECURITY_RESULT_OK).sendFlush();
        }
        else
        {
            const std::string err("no matching security types");
            sendIntBE32(RFB::SECURITY_RESULT_ERR).sendIntBE32(err.size()).sendString(err).sendFlush();
            ERROR("error:" << err);
            return EXIT_FAILURE;
        }

        // RFB 6.3.1 client init
        int clientSharedFlag = recvInt8();
        if(debug)
            DEBUG("RFB 6.3.1, client shared: " << SWE::String::hex(clientSharedFlag, 2));
        // RFB 6.3.2 server init
        sendIntBE16(fbPtr->width());
        sendIntBE16(fbPtr->height());

#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
        bool big_endian = false;
#else
        bool big_endian = true;
#endif
#ifdef SWE_SDL12
        auto serverFormat = PixelFormat(32 /* bpp */, 24 /* depth */, big_endian, true, Surface::defRMask(), Surface::defGMask(), Surface::defBMask());
#else
        uint32_t rmask, gmask, bmask, amask;
        int bpp;

        SDL_PixelFormatEnumToMasks(SWE::Display::defaultPixelFormat(), &bpp, &rmask, &gmask, &bmask, &amask);
        auto serverFormat = PixelFormat(bpp, 24 /* depth */, big_endian, true, rmask, gmask, bmask);
#endif
        if(2 < debug)
        {
            DEBUG("server send: pixel format, bpp: " << (int) serverFormat.bitsPerPixel <<
                ", depth: " << (int) serverFormat.depth <<
                ", big endian: " << (int) serverFormat.bigEndian() <<
                ", true color: " << (int) serverFormat.trueColor() <<
                ", red(" << serverFormat.redMax << ", " << (int) serverFormat.redShift <<
                "), green(" << serverFormat.greenMax << ", " << (int) serverFormat.greenShift <<
                "), blue(" << serverFormat.blueMax << ", " << (int) serverFormat.blueShift << ")");
        }

        // send pixel format
        sendInt8(serverFormat.bitsPerPixel);
        sendInt8(serverFormat.depth);
        sendInt8(serverFormat.bigEndian());
        sendInt8(serverFormat.trueColor());
        sendIntBE16(serverFormat.redMax);
        sendIntBE16(serverFormat.greenMax);
        sendIntBE16(serverFormat.blueMax);
        sendInt8(serverFormat.redShift);
        sendInt8(serverFormat.greenShift);
        sendInt8(serverFormat.blueShift);
        // default client format
        clientFormat = serverFormat;
        clientRegion = fbPtr->region();
        // send padding
        sendInt8(0);
        sendInt8(0);
        sendInt8(0);
        // send name desktop
        const std::string desktopName("MultiCapture");
        sendIntBE32(desktopName.size());
        sendString(desktopName).sendFlush();
        if(debug)
            DEBUG("connector starting: wait RFB messages...");

        while(loopMessage)
        {
            // RFB: mesage loop
            if(hasInput())
            {
                int msgType = recvInt8();

                switch(msgType)
                {
                    case RFB::CLIENT_SET_PIXEL_FORMAT:
                        clientSetPixelFormat();
                        clientUpdateReq = true;
                        break;

                    case RFB::CLIENT_SET_ENCODINGS:
                        if(clientSetEncodings())
                        {
                            // full update
                            clientUpdateReq = true;
                        }
                        break;

                    case RFB::CLIENT_REQUEST_FB_UPDATE:
                        // full update only, skip incremental
                        clientUpdateReq = clientFramebufferUpdate();
                        break;

                    case RFB::CLIENT_EVENT_KEY:
                        clientKeyEvent();
                        clientUpdateReq = true;
                        break;

                    case RFB::CLIENT_EVENT_POINTER:
                        clientPointerEvent();
                        clientUpdateReq = true;
                        break;

                    case RFB::CLIENT_CUT_TEXT:
                        clientCutTextEvent();
                        clientUpdateReq = true;
                        break;

                    default:
                        throw std::runtime_error(std::string("RFB unknown message: ").append(SWE::String::hex(msgType, 2)));
                }
            }

            // server action
            if(! isUpdateProcessed() && clientUpdateReq)
            {
                fbUpdateProcessing = true;

                // background job
                std::thread([this, res = clientRegion]()
                {
                    bool error = false;
                    try
                    {
                        this->serverSendFrameBufferUpdate(res);
                    }
                    catch(const std::exception & err)
                    {
                        ERROR("exception: " << err.what());
                        error = true;
                    }
                    catch(...)
                    {
                        ERROR("exception: " << "unknown");
                        error = true;
                    }

                    this->fbUpdateProcessing = false;

                    if(error)
                        this->loopMessage = false;

                }).detach();
                clientUpdateReq = false;
            }

            // wait
            std::this_thread::sleep_for(5ms);
        }

        return EXIT_SUCCESS;
    }

    void ServerConnector::shutdown(void)
    {
        loopMessage = false;
    }

    bool ServerConnector::isUpdateProcessed(void) const
    {
        return fbUpdateProcessing || ! jobsEncodings.empty();
    }

    void ServerConnector::waitSendingFBUpdate(void) const
    {
        while(isUpdateProcessed())
            std::this_thread::sleep_for(10ms);
    }

    void ServerConnector::clientSetPixelFormat(void)
    {
        // RFB: 6.4.1
        // skip padding
        recvSkip(3);
        auto bitsPerPixel = recvInt8();
        auto depth = recvInt8();
        auto bigEndian = recvInt8();
        auto trueColor = recvInt8();
        auto redMax = recvIntBE16();
        auto greenMax = recvIntBE16();
        auto blueMax = recvIntBE16();
        auto redShift = recvInt8();
        auto greenShift = recvInt8();
        auto blueShift = recvInt8();
        // skip padding
        recvSkip(3);

        if(debug)
        {
            DEBUG("RFB 6.4.1, set pixel format, bpp: " << (int) bitsPerPixel <<
                ", depth: " << (int) depth <<
                ", big endian: " << (int) bigEndian <<
                ", true color: " << (int) trueColor <<
                ", red(" << redMax << ", " << (int) redShift <<
                "), green(" << greenMax << ", " << (int) greenShift <<
                "), blue(" << blueMax << ", " << (int) blueShift << ")");
        }

        switch(bitsPerPixel)
        {
            case 32:
            case 16:
            case 8:
                break;

            default:
                throw std::runtime_error("unknown client pixel format");
        }

        if(trueColor == 0 || redMax == 0 || greenMax == 0 || blueMax == 0)
            throw std::runtime_error("unsupported pixel format");

        clientFormat = PixelFormat(bitsPerPixel, depth, bigEndian, trueColor, redMax, greenMax, blueMax, redShift, greenShift, blueShift);
    }

    bool ServerConnector::clientSetEncodings(void)
    {
        // RFB: 6.4.2
        // skip padding
        recvSkip(1);
        int previousType = prefEncodings.second;
        int numEncodings = recvIntBE16();
        if(debug)
            DEBUG("RFB 6.4.2, set encodings, counts: " << numEncodings);
        clientEncodings.clear();
        clientEncodings.reserve(numEncodings);

        while(0 < numEncodings--)
        {
            int encoding = recvIntBE32();
            clientEncodings.push_back(encoding);

            if(1 < debug)
            {
                DEBUG("RFB request encodings: " << RFB::encodingName(encoding));
            }
        }

        prefEncodings = selectEncodings();
        DEBUG("server select encoding: " << RFB::encodingName(prefEncodings.second));

        if(std::any_of(clientEncodings.begin(), clientEncodings.end(),
                    [=](auto & val){ return val == RFB::ENCODING_CONTINUOUS_UPDATES; }))
        {
            // RFB 1.7.7.15
            // The server must send a EndOfContinuousUpdates message the first time
            // it sees a SetEncodings message with the ContinuousUpdates pseudo-encoding,
            // in order to inform the client that the extension is supported.
            //
            DEBUG("ENCODING_CONTINUOUS_UPDATES present!");
            // waitSendingFBUpdate();
            // serverSendEndContinuousUpdates();
        }

        return previousType != prefEncodings.second;
    }

    void ServerConnector::clientEnableContinuousUpdates(void)
    {
        int enable = recvInt8();
        int regx = recvIntBE16();
        int regy = recvIntBE16();
        int regw = recvIntBE16();
        int regh = recvIntBE16();

        if(debug)
        {
            DEBUG("RFB 1.7.4.7, enable continuous updates, region: [" <<
                    regx << ", " << regy << ", " << regw << ", " << regh <<", enabled: " << enable);
        }

        throw std::runtime_error("clientEnableContinuousUpdates: not implemented");
    }

    bool ServerConnector::clientFramebufferUpdate(void)
    {
        // RFB: 6.4.3
        int incremental = recvInt8();
        clientRegion.x = recvIntBE16();
        clientRegion.y = recvIntBE16();
        clientRegion.w = recvIntBE16();
        clientRegion.h = recvIntBE16();
        if(debug)
        {
            DEBUG("RFB 6.4.3, request update fb, region [" <<
                clientRegion.x << ", " << clientRegion.y << ", " << clientRegion.w << ", " << clientRegion.h <<
                "], incremental: " << incremental);
        }
        bool fullUpdate = incremental == 0;
        auto serverRegion = fbPtr->region();

        if(fullUpdate)
            clientRegion = serverRegion;
        else
        {
            clientRegion = serverRegion.intersected(clientRegion);

            if(clientRegion.toSize().isEmpty())
                ERROR("client region intersection with display [" << serverRegion.w << ", " << serverRegion.h << "] failed");
        }

        return fullUpdate;
    }

    void ServerConnector::clientKeyEvent(void)
    {
        // RFB: 6.4.4
        int pressed = recvInt8();
        recvSkip(2);
        int keysym = recvIntBE32();
        if(2 < debug)
            DEBUG("RFB 6.4.4, key event ( " << (pressed ? "pressed" : "released") << "), keysym: " << SWE::String::hex(keysym, 4));
    }

    void ServerConnector::clientPointerEvent(void)
    {
        // RFB: 6.4.5
        int mask = recvInt8(); // button1 0x01, button2 0x02, button3 0x04
        int posx = recvIntBE16();
        int posy = recvIntBE16();
        if(2 < debug)
            DEBUG("RFB 6.4.5, pointer event, mask: " << SWE::String::hex(mask, 2) << ", posx: " << posx << ", posy: " << posy);
    }

    void ServerConnector::clientCutTextEvent(void)
    {
        // RFB: 6.4.6
        // skip padding
        recvSkip(3);
        size_t length = recvIntBE32();
        if(debug)
            DEBUG("RFB 6.4.6, cut text event, length: " << length);
        recvSkip(length);
    }

    void ServerConnector::serverSendBell(void)
    {
        const std::lock_guard<std::mutex> lock(sendGlobal);
        if(debug)
            DEBUG("server send: bell");
        // RFB: 6.5.3
        sendInt8(RFB::SERVER_BELL);
        sendFlush();
    }

    void ServerConnector::serverSendEndContinuousUpdates(void)
    {
        // RFB: 1.7.5.5
        sendInt8(RFB::CLIENT_ENABLE_CONTINUOUS_UPDATES).sendFlush();
    }

    bool ServerConnector::serverSendFrameBufferUpdate(const Region & reg)
    {
        const std::lock_guard<std::mutex> lock(sendGlobal);
        if(debug)
            DEBUG("server send fb update");

        // RFB: 6.5.1
        sendInt8(RFB::SERVER_FB_UPDATE);
        // padding
        sendInt8(0);

        // send encodings
        prefEncodings.first(*fbPtr);

        sendFlush();
        return true;
    }

    int ServerConnector::sendPixel(uint32_t pixel)
    {
        if(clientFormat.trueColor())
        {
            switch(clientFormat.bytePerPixel())
            {
                case 4:
                    if(clientFormat.bigEndian())
                        sendIntBE32(clientFormat.convertFrom(fbPtr->pixelFormat(), pixel));
                    else
                        sendIntLE32(clientFormat.convertFrom(fbPtr->pixelFormat(), pixel));

                    return 4;

                case 2:
                    if(clientFormat.bigEndian())
                        sendIntBE16(clientFormat.convertFrom(fbPtr->pixelFormat(), pixel));
                    else
                        sendIntLE16(clientFormat.convertFrom(fbPtr->pixelFormat(), pixel));

                    return 2;

                case 1:
                    sendInt8(clientFormat.convertFrom(fbPtr->pixelFormat(), pixel));
                    return 1;

                default:
                    break;
            }
        }

        throw std::runtime_error("send pixel: unknown format");
        return 0;
    }

    int ServerConnector::sendCPixel(uint32_t pixel)
    {
        if(clientFormat.trueColor() && clientFormat.bitsPerPixel == 32)
        {
            auto pixel2 = clientFormat.convertFrom(fbPtr->pixelFormat(), pixel);
            auto red = clientFormat.red(pixel2);
            auto green = clientFormat.green(pixel2);
            auto blue = clientFormat.blue(pixel2);
#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            std::swap(red, blue);
#endif
            sendInt8(red);
            sendInt8(green);
            sendInt8(blue);
            return 3;
        }

        return sendPixel(pixel);
    }

    int ServerConnector::sendRunLength(size_t length)
    {
        int res = 0;
        while(255 < length)
        {
            sendInt8(255);
            res += 1;
            length -= 255;
        }

        sendInt8((length - 1) % 255);
        return res + 1;
    }

    void ServerConnector::zlibDeflateStart(size_t len)
    {
        auto it = std::find_if(clientEncodings.begin(), clientEncodings.end(),
                    [=](auto & val){ return ENCODING_COMPRESS1 <= val && val <= ENCODING_COMPRESS9; });
        int level = it != clientEncodings.end() ? ENCODING_COMPRESS9 - *it : 9;

        if(! zlib)
            zlib.reset(new Network::DeflateStream(level));

        zlib->prepareSize(len);
        streamOut = zlib.get();
    }

    int ServerConnector::zlibDeflateStop(bool uint16sz)
    {
        streamOut = socket.get();
        auto zip = zlib->deflateFlush();
        int res = 0;

        if(uint16sz)
        {
            sendIntBE16(zip.size());
            res = 2;
        }
        else
        {
            sendIntBE32(zip.size());
            res = 4;
        }
        sendRaw(zip.data(), zip.size());
        return res + zip.size();
    }

    void ServerConnector::setFrameBuffer(const SWE::Surface & surf)
    {
        if(surf.isValid() && (!fbPtr || fbSurf != surf))
        {
            const std::lock_guard<std::mutex> lock(sendGlobal);
            fbSurf = surf;

#if (__BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__)
            bool bigEndian = false;
#else
            bool bigEndian = true;
#endif
            SDL_Surface* sf = fbSurf.toSDLSurface();
            auto fmt = sf->format;

            auto ptr = new FrameBuffer((uint8_t*) sf->pixels, Region(0, 0, sf->w, sf->h),
                                    PixelFormat(fmt->BitsPerPixel, 24 /* vnc fixed depth */, bigEndian, true, fmt->Rmask, fmt->Gmask, fmt->Bmask), sf->pitch);

            fbPtr.reset(ptr);

            clientUpdateReq = true;
            clientRegion = fbPtr->region();
        }
    }
}
