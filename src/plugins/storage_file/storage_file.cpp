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

#include <sys/stat.h>

#include <mutex>
#include <algorithm>

#include "../../settings.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef SWE_SDL12
#include "SDL_rotozoom.h"
#else
#include "SDL2_rotozoom.h"
#endif

const int storage_file_version = 20220415;

struct storage_file_t
{
    int         debug;
    bool        overwrite;
    bool        deinterlace;
    size_t      sessionId;
    std::string sessionName;
    std::string format;
    std::string filename;
    Surface	surface;
    Size        geometry;
    std::mutex  change;

    storage_file_t() : debug(0), overwrite(false), deinterlace(false), sessionId(0) {}
    ~storage_file_t()
    {
	clear();
    }

    void clear(void)
    {
        debug = 0;
        overwrite = false;
        deinterlace = false;
        sessionId = 0;
        sessionName.clear();
        format.clear();
        filename.clear();
	surface.reset();
    }
};

void* storage_file_init(const JsonObject & config)
{
    VERBOSE("version: " << storage_file_version);

    auto ptr = std::make_unique<storage_file_t>();

    ptr->debug = config.getInteger("debug", 0);
    ptr->overwrite = config.getBoolean("overwrite", false);
    ptr->deinterlace = config.getBoolean("deinterlace", false);
    ptr->format = config.getString("format");
    ptr->geometry = JsonUnpack::size(config, "size");

    if(ptr->format.empty())
        ptr->format = config.getString("filename");

    if(! ptr->geometry.isEmpty())
        DEBUG("params: " << "geometry = " << ptr->geometry.toString());

    if(ptr->format.empty())
    {
        ERROR("filename params empty");
        ptr->clear();
        return nullptr;
    }

    DEBUG("params: " << "filename = " << ptr->format);

    return ptr.release();
}

void storage_file_quit(void* ptr)
{
    storage_file_t* st = static_cast<storage_file_t*>(ptr);
    if(st->debug) DEBUG("version: " << storage_file_version);

    delete st;
}

Surface storage_surface_deinterlace(const Surface & back)
{
    auto sf1 = back.toSDLSurface();
    auto sf2 = zoomSurface(sf1, 1.0, 0.5, 0);
    auto sf3 = zoomSurface(sf2, 1.0, 2.0, 1);
    SDL_FreeSurface(sf2);

    return Surface(sf3);
}

Surface storage_surface_scale(const Surface & back, const Size & nsz)
{
    auto sf1 = back.toSDLSurface();
    double ratioWidth = nsz.w / static_cast<double>(sf1->w);
    double ratioHeight = nsz.h / static_cast<double>(sf1->h);

    auto sf2 = zoomSurface(sf1, ratioWidth, ratioHeight, 1 /* smooth */);
    return Surface(sf2);
}

// PluginResult::Reset, PluginResult::Failed, PluginResult::DefaultOk, PluginResult::NoAction
int storage_file_store_action(void* ptr, const std::string & signal)
{
    storage_file_t* st = static_cast<storage_file_t*>(ptr);
    if(! st->surface.isValid())
    {
        ERROR("invalid surface");
        return PluginResult::Failed;
    }

    if(3 < st->debug) DEBUG("version: " << storage_file_version);

    if(true)
    {
        const std::lock_guard<std::mutex> lock(st->change);
        st->filename = String::strftime(st->format);

        if(0 < st->sessionId)
    	    st->filename = String::replace(st->filename, "${sid}", st->sessionId);

        if(! st->sessionName.empty())
    	    st->filename = String::replace(st->filename, "${session}", st->sessionName);
    }

    std::string dir = Systems::dirname(st->filename);

    if(! Systems::isDirectory(dir))
	Systems::makeDirectory(dir, 0775);

    if(! Systems::isFile(st->filename) || st->overwrite)
    {
        const std::lock_guard<std::mutex> lock(st->change);

        if(2 < st->debug)
	    DEBUG("save: " << st->filename);

#ifndef SDL_SWE12
        try
        {
            if(st->deinterlace)
                st->surface = storage_surface_deinterlace(st->surface);

            if(! st->geometry.isEmpty())
                st->surface = storage_surface_scale(st->surface, st->geometry);
        }
        catch(const std::exception & err)
        {
            ERROR(err.what());
        }
#endif

	st->surface.save(st->filename);

        // backup to home
	if(! Systems::isFile(st->filename))
        {
            st->filename = Systems::concatePath(Systems::environment("HOME"), Systems::basename(st->filename));
	    ERROR("save to backup: " << st->filename);
            st->surface.save(st->filename);
        }
/*
	if(Systems::isFile(st->filename) && ! storeSignal.empty())
            DisplayScene::pushEvent(nullptr, ActionStorageSignal, st);
*/
    }
    else
    {
	ERROR("file present: " << st->filename << ", overwrite skipping...");
    }

    return PluginResult::DefaultOk;
}

bool storage_file_get_value(void* ptr, int type, void* val)
{
    switch(type)
    {
        case PluginValue::PluginName:
            if(auto res = static_cast<std::string*>(val))
            {
                res->assign("storage_file");
                return true;
            }
            break;
    
        case PluginValue::PluginVersion:
            if(auto res = static_cast<int*>(val))
            {
                *res = storage_file_version;
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
        storage_file_t* st = static_cast<storage_file_t*>(ptr);
        if(4 < st->debug)
            DEBUG("version: " << storage_file_version << ", type: " << type);
    
        switch(type)
        {
            case PluginValue::StorageLocation:
                if(auto res = static_cast<std::string*>(val))
                {
                    const std::lock_guard<std::mutex> lock(st->change);
                    res->assign(st->filename);
                    return true;
                }
                break;

            case PluginValue::StorageSurface:
                if(auto res = static_cast<Surface*>(val))
                {
                    const std::lock_guard<std::mutex> lock(st->change);
                    *res = st->surface;
                    return true;
                }
                break;

            default: break;
        }
    }

    return false;
}

bool storage_file_set_value(void* ptr, int type, const void* val)
{
    storage_file_t* st = static_cast<storage_file_t*>(ptr);
    if(4 < st->debug)
        DEBUG("version: " << storage_file_version << ", type: " << type);

    switch(type)
    {
        case PluginValue::StorageSurface:
            if(auto res = static_cast<const Surface*>(val))
            {
                const std::lock_guard<std::mutex> lock(st->change);
                st->surface = *res;
                return true;
            }
            break;

        case PluginValue::SessionId:
            if(auto res = static_cast<const size_t*>(val))
            {
                const std::lock_guard<std::mutex> lock(st->change);
                st->sessionId = *res;
                return true;
            }
            break;

        case PluginValue::SessionName:
            if(auto res = static_cast<const std::string*>(val))
            {
                const std::lock_guard<std::mutex> lock(st->change);
                st->sessionName.assign(*res);
                return true;
            }
            break;

        default: break;
    }

    return false;
}

#ifdef __cplusplus
}
#endif
