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

#include <mutex>
#include "../../settings.h"

#ifdef __cplusplus
extern "C" {
#endif

const int storage_script_version = 20220412;

struct storage_script_t
{
    int         debug;
    size_t      sessionId;
    std::string sessionName;
    std::string command;
    std::string format;
    std::string filename;
    Surface	surface;
    std::mutex  change;

    storage_script_t() : debug(0), sessionId(0) {}
    ~storage_script_t()
    {
	clear();
    }
    
    void clear(void)
    {
        debug = 0;
        sessionId = 0;
        sessionName.clear();
        command.clear();
        filename.clear();
	surface.reset();
    }
};

void* storage_script_init(const JsonObject & config)
{
    VERBOSE("version: " << storage_script_version);

    auto ptr = std::make_unique<storage_script_t>();

    ptr->debug = config.getInteger("debug", 0);
    ptr->command = config.getString("exec");

    ptr->format = config.getString("image");
    if(ptr->format.empty())
        ptr->format = config.getString("filename");

    if(ptr->format.empty() || ptr->command.empty())
    {
        if(ptr->format.empty())
            ERROR("filename param empty");
        if(ptr->command.empty())
            ERROR("exec param empty");
        ptr->clear();
        return nullptr;
    }

    auto list = String::split(ptr->command, 0x20);
    if(! Systems::isFile(list.front()))
    {
        ERROR("not command present: " << ptr->command);
        ptr->clear();
        return nullptr;
    }

    DEBUG("params: " << "exec = " << ptr->command);
    DEBUG("params: " << "filename = " << ptr->format);

    return ptr.release();
}

void storage_script_quit(void* ptr)
{
    storage_script_t* st = static_cast<storage_script_t*>(ptr);
    if(st->debug) DEBUG("version: " << storage_script_version);

    delete st;
}

// PluginResult::Reset, PluginResult::Failed, PluginResult::DefaultOk, PluginResult::NoAction
int storage_script_store_action(void* ptr, const std::string & signal)
{
    storage_script_t* st = static_cast<storage_script_t*>(ptr);
    if(! st->surface.isValid())
    {
        ERROR("invalid surface");
        return PluginResult::Failed;
    }

    if(3 < st->debug) DEBUG("version: " << storage_script_version);

    if(true)
    {
        const std::lock_guard<std::mutex> lock(st->change);
        st->filename = String::strftime(st->format);

        if(0 < st->sessionId)
            st->filename = String::replace(st->filename, "${sid}", st->sessionId);
    
        if(! st->sessionName.empty())
            st->filename = String::replace(st->filename, "${session}", st->sessionName);
    }

    if(Systems::isFile(st->command))
    {
        const std::lock_guard<std::mutex> lock(st->change);

        if(2 < st->debug)
    	    DEBUG("save: " << st->filename);

    	st->surface.save(st->filename);

    	if(! Systems::isFile(st->filename))
    	{
	    ERROR("write image error");
	    return PluginResult::Failed;
    	}

    	std::string cmd = st->command;
    	cmd.append(" ").append(st->filename);
    	system(cmd.c_str());

    	return PluginResult::DefaultOk;
    }
    else
    {
        ERROR("command not found: " << st->command);
    }

    return PluginResult::Failed;
}

bool storage_script_get_value(void* ptr, int type, void* val)
{
    switch(type)
    {
        case PluginValue::PluginName:
            if(auto res = static_cast<std::string*>(val))
            {
                res->assign("storage_script");
                return true;
            }
            break;
    
        case PluginValue::PluginVersion:
            if(auto res = static_cast<int*>(val))
            {
                *res = storage_script_version;
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
        storage_script_t* st = static_cast<storage_script_t*>(ptr);
        if(4 < st->debug)
            DEBUG("version: " << storage_script_version << ", type: " << type);
    
        switch(type)
        {
            case PluginValue::StorageLocation:
                if(auto res = static_cast<std::string*>(val))
                {
                    res->assign(st->filename);
                    return true;
                }
                break;

            case PluginValue::StorageSurface:
                if(auto res = static_cast<Surface*>(val))
                {
		    if(res->isValid())
		    {
                	const std::lock_guard<std::mutex> lock(st->change);
            		res->setSurface(st->surface);
                	return true;
		    }
                }
                break;

            default: break;
        }
    }

    return false;
}

bool storage_script_set_value(void* ptr, int type, const void* val)
{
    storage_script_t* st = static_cast<storage_script_t*>(ptr);
    if(4 < st->debug)
        DEBUG("version: " << storage_script_version << ", type: " << type);

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
