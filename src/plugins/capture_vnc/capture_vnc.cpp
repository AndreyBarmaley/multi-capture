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
#include <memory>

#include "../../settings.h"
#include "capture_vnc_connector.h"

#ifdef __cplusplus
extern "C" {
#endif

using namespace std::chrono_literals;
const int capture_vnc_version = 20220412;

struct capture_vnc_t : RFB::ClientConnector
{
    int 	debug;
    int         port;
    std::string host;
    std::string password;

    std::thread thread;
    Frames      frames;

    capture_vnc_t(const JsonObject & config) : RFB::ClientConnector(config), debug(0), port(5900)
    {
    }

    ~capture_vnc_t()
    {
	clear();
    }

    void clear(void)
    {
        shutdown();
        if(thread.joinable()) thread.join();

	debug = 0;
        port = 5900;
        host.clear();
        password.clear();
    }

    void serverFBUpdateEvent(void) override
    {
        RFB::ClientConnector::serverFBUpdateEvent();

        if(frames.size() < 3)
        {
            Surface frame;
            syncFrameBuffer(frame);
            frames.push_back(frame);

            DisplayScene::pushEvent(nullptr, ActionFrameComplete, this);
        }
    }

    bool init(void)
    {
        bool handshake = false;

        try
        {
            handshake = communication(host, port, password);
        }
        catch(const std::exception & err)
        {
            ERROR("exception: " << err.what());
        }
        catch(...)
        {
            ERROR("exception: " << "unknown");
        }

        if(! handshake)
        {
            DisplayScene::pushEvent(nullptr, ActionCaptureReset, this);
            return false;
        }

        thread = std::thread([st = this]
        {
            try
            {
                st->messages();
            }
            catch(const std::exception & err)
            {
                ERROR("exception: " << err.what());
            }
            catch(...)
            {
                ERROR("exception: " << "unknown");
            }

            DisplayScene::pushEvent(nullptr, ActionCaptureReset, st);
        });

        return handshake;
    }
};

void* capture_vnc_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_vnc_version);

    auto ptr = std::make_unique<capture_vnc_t>(config);

    ptr->debug = config.getInteger("debug", 0);
    ptr->port = config.getInteger("port", 5900);
    ptr->host = config.getString("host");
    ptr->password = config.getString("password");

    if(ptr->host.empty())
    {
        ERROR("host param empty");
        ptr->clear();
        return nullptr;
    }

    if(ptr->debug)
    {
        DEBUG("params: " << "host = " << ptr->host);
        DEBUG("params: " << "port = " << ptr->port);
    }

    if(! ptr->init())
    {
        ptr->clear();
        return nullptr;
    }

    return ptr.release();
}

void capture_vnc_quit(void* ptr)
{
    capture_vnc_t* st = static_cast<capture_vnc_t*>(ptr);
    if(st->debug) DEBUG("version: " << capture_vnc_version);

    delete st;
}

bool capture_vnc_get_value(void* ptr, int type, void* val)
{
    switch(type)
    {
        case PluginValue::PluginName:
            if(auto res = static_cast<std::string*>(val))
            {
                res->assign("capture_vnc");
                return true;
            }
            break;

        case PluginValue::PluginVersion:
            if(auto res = static_cast<int*>(val))
            {
                *res = capture_vnc_version;
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
        capture_vnc_t* st = static_cast<capture_vnc_t*>(ptr);
        if(4 < st->debug)
            DEBUG("version: " << capture_vnc_version << ", type: " << type);

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

bool capture_vnc_set_value(void* ptr, int type, const void* val)
{
    capture_vnc_t* st = static_cast<capture_vnc_t*>(ptr);
    if(4 < st->debug)
        DEBUG("version: " << capture_vnc_version << ", type: " << type);

    switch(type)
    {
        default: break;
    }

    return false;
}

#ifdef __cplusplus
}
#endif
