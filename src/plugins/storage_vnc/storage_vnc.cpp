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
#include <atomic>
#include <thread>
#include <memory>

using namespace std::chrono_literals;

#include "../../settings.h"
#include "storage_vnc_connector.h"

#ifdef __cplusplus
extern "C" {
#endif

const int storage_vnc_version = 20220415;

struct storage_vnc_t
{
    int        debug;
    Network::TCPServer sn;
    int         port;
    std::thread	threadVncCommunication;
    std::atomic<bool> vncThreadShutdown;
    bool frameBufferReceived;
    const SWE::JsonObject* config;
    std::unique_ptr<RFB::ServerConnector> vnc;

    storage_vnc_t() : debug(0), port(0), vncThreadShutdown(false), frameBufferReceived(false), config(nullptr)
    {
    }

    ~storage_vnc_t()
    {
        clear();
    }

    static void start_thread(storage_vnc_t* st)
    {
        // wait accept
        TCPsocket sock = nullptr;
        DEBUG("start thread");

        while(! st->vncThreadShutdown)
        {
            // wait accept
            while(! st->vncThreadShutdown)
            {
                sock = st->sn.accept();
                if(sock) break;
                std::this_thread::sleep_for(50ms);
            }

            if(! sock)
            {
                ERROR("socket is null");
                std::this_thread::sleep_for(50ms);
                continue;
            }

            VERBOSE("client: connected");
            st->vnc.reset(new RFB::ServerConnector(sock, st->config));

            // wait set surface
            while(! st->frameBufferReceived)
                std::this_thread::sleep_for(100ms);

            if(! st->vncThreadShutdown)
            {
                IPaddress* ipa = sock ? SDLNet_TCP_GetPeerAddress(sock) : nullptr;
                std::string peer = StringFormat("%4.%3.%2.%1").
                               arg(ipa ? 0xFF & (ipa->host >> 24) : 0).arg(ipa ? 0xFF & (ipa->host >> 16) : 0).
                               arg(ipa ? 0xFF & (ipa->host >> 8) : 0).arg(ipa ? 0xFF & ipa->host : 0);

                try
                {
                    st->vnc->communication(peer);
                }
                catch(const std::exception & err)
                {
                    ERROR("exception: " << err.what());
                }
                catch(...)
                {
                    ERROR("exception: " << "unknown");
                }

                st->vnc.reset();
                st->frameBufferReceived = false;
            }

            VERBOSE("client: disconnected");
        }
    }

    bool init(void)
    {
        if(! sn.listen(port))
        {
            ERROR("listen port: " << port);
            return false;
        }

        threadVncCommunication = std::thread([this]()
        {
            start_thread(this);
        });
        return true;
    }

    void clear(void)
    {
        vncThreadShutdown = true;
        if(threadVncCommunication.joinable())
            threadVncCommunication.join();

        debug = 0;
        sn.close();
        config = nullptr;
        vncThreadShutdown = false;
        frameBufferReceived = false;
    }
};

void* storage_vnc_init(const JsonObject & config)
{
    VERBOSE("version: " << storage_vnc_version);
    auto ptr = std::make_unique<storage_vnc_t>();
    ptr->debug = config.getInteger("debug", 0);
    ptr->port = config.getInteger("port", 5900);
    bool noauth = config.getBoolean("noauth");
    std::string passwdfile = config.getString("passwdfile");
    ptr->config = & config;

    DEBUG("params: " << "port = " << ptr->port);
    DEBUG("params: " << "noauth = " << String::Bool(noauth));
    if(! passwdfile.empty())
        DEBUG("params: " << "passwdfile = " << passwdfile);

    if(! ptr->init())
    {
        ptr->clear();
        return nullptr;
    }

    return ptr.release();
}

void storage_vnc_quit(void* ptr)
{
    storage_vnc_t* st = static_cast<storage_vnc_t*>(ptr);

    if(st->debug) DEBUG("version: " << storage_vnc_version);

    st->vncThreadShutdown = true;
    if(st->vnc) st->vnc->shutdown();

    delete st;
}

// PluginResult::Reset, PluginResult::Failed, PluginResult::DefaultOk, PluginResult::NoAction
int storage_vnc_store_action(void* ptr, const std::string & signal)
{
    storage_vnc_t* st = static_cast<storage_vnc_t*>(ptr);
    if(3 < st->debug) DEBUG("version: " << storage_vnc_version);

    // always store
    return PluginResult::NoAction;
}

int storage_vnc_session_reset(void* ptr, const SessionIdName & ss)
{
    storage_vnc_t* st = static_cast<storage_vnc_t*>(ptr);

    if(st->debug) DEBUG("version: " << storage_vnc_version);

    return 0;
}

bool storage_vnc_get_value(void* ptr, int type, void* val)
{
    switch(type)
    {
        case PluginValue::PluginName:
            if(auto res = static_cast<std::string*>(val))
            {
                res->assign("storage_vnc");
                return true;
            }
            break;
    
        case PluginValue::PluginVersion:
            if(auto res = static_cast<int*>(val))
            {
                *res = storage_vnc_version;
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
        storage_vnc_t* st = static_cast<storage_vnc_t*>(ptr);
        if(4 < st->debug)
            DEBUG("version: " << storage_vnc_version << ", type: " << type);

        switch(type)
        {
            case PluginValue::StorageLocation:
                if(auto res = static_cast<std::string*>(val))
                {
                    res->assign("vnc:").append(std::to_string(st->port));
                    return true;
                }
                break;

            case PluginValue::StorageSurface:
                return false;

            default: break;
        }
    }

    return false;
}

bool storage_vnc_set_value(void* ptr, int type, const void* val)
{
    storage_vnc_t* st = static_cast<storage_vnc_t*>(ptr);
    if(4 < st->debug)
        DEBUG("version: " << storage_vnc_version << ", type: " << type);

    switch(type)
    {
        case PluginValue::StorageSurface:
            if(auto res = static_cast<const Surface*>(val))
            {
		if(! res->isValid())
		    return false;

                if(st->vnc)
		{
                    st->vnc->setFrameBuffer(*res);
            	    st->frameBufferReceived = true;
            	    return true;
		}
            }
            break;

        default: break;
    }

    return false;
}

#ifdef __cplusplus
}
#endif
