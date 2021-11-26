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

#include <chrono>
#include <algorithm>

#include "mainscreen.h"
#include "settings.h"
#include "plugins.h"

using namespace std::chrono_literals;

/* PluginParams */
PluginParams::PluginParams(const JsonObject & jo)
{
    name = jo.getString("name");
    type = jo.getString("type");
    file = jo.getString("file");

    if(name.empty())
	name = type;

    if(file.size() && ! Systems::isFile(file))
    {
	ERROR("plugin not found: " << file);
	if(type.size()) file.clear();
    }

    // find type.so
    if(type.size() && file.empty())
    {
	StringList dirs;
	dirs << "plugins" << "libs";

	for(auto & dir : Systems::shareDirectories(Settings::programDomain()))
	    dirs << Systems::concatePath(dir, "plugins");

	for(auto & dir : dirs)
	{
	    std::string filename = Systems::concatePath(dir, type).append(Systems::suffixLib());
	    if(Systems::isFile(filename))
	    {
		VERBOSE("plugin found: " << name << ", (" << filename << ")");
		file = filename;
		break;
	    }
	}
    }

    if(jo.isString("config"))
    {
        JsonContentFile json(jo.getString("config"));

        if(json.isValid() && json.isObject())
            config = json.toObject();
    }
    else
    {
	config = jo;
    }
}

/* BasePlugin */
BasePlugin::BasePlugin(const PluginParams & params, Window & win) : PluginParams(params), lib(nullptr), data(nullptr),
    fun_get_name(nullptr), fun_get_version(nullptr), fun_init(nullptr), fun_quit(nullptr),
    threadInitialize(false), threadAction(false), threadExit(false), threadResult(0), parent(& win)
{
    lib = Systems::openLib(file);

    if(lib)
    {
	DEBUG("open library: " << file);
    }
    else
    {
	ERROR("cannot open library: " << file);
    }

    loadFunctions();
}

BasePlugin::~BasePlugin()
{
    if(thread.joinable())
    {
	threadExit = true;
        std::this_thread::sleep_for(50ms);

        DEBUG("wait thread: " << name);
        thread.join();
    }

    if(lib)
    {
        if(fun_quit && data)
	    fun_quit(data);
	Systems::closeLib(lib);
    }
}

void BasePlugin::stopThread(void)
{
	threadExit = true;
}

void BasePlugin::joinThread(void)
{
    if(thread.joinable())
    {
	threadExit = true;
        std::this_thread::sleep_for(200ms);

        DEBUG("wait thread: " << name);
        thread.join();
    }
}

bool BasePlugin::loadFunctions(void)
{
    if(! isValid())
	return false;

    std::string str;

    // fun_init
    str = std::string(type).append("_init");
    fun_init = (void* (*)(const JsonObject &)) Systems::procAddressLib(lib, str);
    if(! fun_init)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_quit
    str = std::string(type).append("_quit");
    fun_quit = (void (*)(void*)) Systems::procAddressLib(lib, str);
    if(! fun_quit)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_name
    str = std::string(type).append("_get_name");
    fun_get_name = (const char* (*)(void)) Systems::procAddressLib(lib, str);
    if(! fun_get_name)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_version
    str = std::string(type).append("_get_version");
    fun_get_version = (int (*)(void)) Systems::procAddressLib(lib, str);
    if(! fun_get_version)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    return true;
}

const char* BasePlugin::pluginName(void) const
{
    return fun_get_name ? fun_get_name() : nullptr;
}

int BasePlugin::pluginVersion(void) const
{
    return fun_get_version ? fun_get_version() : 0;
}

/* CapturePlugin */
CapturePlugin::CapturePlugin(const PluginParams & params, Window & win) : BasePlugin(params, win),
    fun_frame_action(nullptr), fun_get_surface(nullptr)
{
    blue = generateBlueScreen(_("error"));

    if(loadFunctions())
    {
        blue = generateBlueScreen(_("initialize"));

	thread = std::thread([this](){
            if(this->data)
                this->fun_quit(this->data);

	    this->threadInitialize = false;
            DEBUG("thread init start: " << this->name);

	    while(! this->threadExit)
	    {
	        if(nullptr != (this->data = this->fun_init(this->config)))
                {
		    this->threadInitialize = true;
                    DEBUG("thread init complete: " << this->name);
		    break;
	        }

                // free res
                ERROR("thread init broken: " << this->name);
                std::this_thread::sleep_for(3000ms);
	    }
        });
    }
}

CapturePlugin::~CapturePlugin()
{
    joinThread();
}

Surface CapturePlugin::generateBlueScreen(const std::string & label) const
{
    Size winsz = JsonUnpack::size(config, "window:size");
    auto res = Surface(winsz);
    res.clear(Color::Blue);
    auto screen = static_cast<MainScreen*>(parent->parent());
    if(screen)
    {
        const FontRender & frs = screen->fontRender();
        Size sfsz = frs.stringSize(label);
        frs.renderString(label, Color::White, (winsz - sfsz) / 2, res);
    }
    return res;
}

bool CapturePlugin::loadFunctions(void)
{
    if(! isValid())
	return false;

    std::string str;

    // fun_action
    str = std::string(type).append("_frame_action");
    fun_frame_action = (int (*)(void*)) Systems::procAddressLib(lib, str);
    if(! fun_frame_action)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_get_surface
    str = std::string(type).append("_get_surface");
    fun_get_surface = (const Surface & (*)(void*)) Systems::procAddressLib(lib, str);
    if(! fun_get_surface)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    return true;
}

int CapturePlugin::frameAction(void)
{
    if(threadInitialize && fun_frame_action && data)
    {
        if(threadAction)
        {
            // wait action complete
            return 0;
        }
        else
        {
            threadAction = true;
            //DEBUG("thread action start: " << name);
	    if(thread.joinable())
		thread.join();
            thread = std::thread([this](){
                int err = this->fun_frame_action(this->data);
                DisplayScene::pushEvent(this->parent, err ? ActionPluginReset : ActionFrameComplete, nullptr);
                this->threadAction = false;
                this->threadResult = err;
            });
            return 0;
        }
    }

    DisplayScene::pushEvent(parent, ActionFrameComplete, nullptr);
    return 1;
}

const Surface & CapturePlugin::getSurface(void)
{
    if(fun_get_surface && data)
    {
	if(threadInitialize)
        {
	    if(threadAction)
	    {
    	        if(thread.joinable())
                    thread.join();
                threadAction = false;
	    }

	    if(0 == threadResult)
	        return fun_get_surface(data);
        }
    }

    // return blue screen: no signal
    return blue;
}

/* StoragePlugin */
StoragePlugin::StoragePlugin(const PluginParams & params, Window & win) : BasePlugin(params, win),
    fun_store_action(nullptr), fun_set_surface(nullptr)
{
    signals = params.config.getStdList<std::string>("signals");
    std::transform(signals.begin(), signals.end(), signals.begin(), String::toLower);

    if(loadFunctions())
    {
	thread = std::thread([this](){
            if(this->data)
                this->fun_quit(this->data);

            this->threadInitialize = false;
            DEBUG("thread init start: " << this->name);

	    while(! this->threadExit)
	    {
	        if(nullptr != (this->data = this->fun_init(this->config)))
                {
		    this->threadInitialize = true;
                    DEBUG("thread init complete: " << this->name);
		    break;
	        }

                // free res
                ERROR("thread init broken: " << this->name);
                std::this_thread::sleep_for(3000ms);
	    }
        });
    }
}

StoragePlugin::~StoragePlugin()
{
    joinThread();
}

bool StoragePlugin::loadFunctions(void)
{
    if(! isValid())
	return false;

    std::string str;

    // fun_store_action
    str = std::string(type).append("_store_action");
    fun_store_action = (int (*)(void*)) Systems::procAddressLib(lib, str);
    if(! fun_store_action)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_set_surface
    str = std::string(type).append("_set_surface");
    fun_set_surface = (int (*)(void*, const Surface &)) Systems::procAddressLib(lib, str);
    if(! fun_set_surface)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_get_label
    str = std::string(type).append("_get_label");
    fun_get_label = (const std::string & (*)(void*)) Systems::procAddressLib(lib, str);
    if(! fun_get_label)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_get_surface
    str = std::string(type).append("_get_surface");
    fun_get_surface = (const Surface & (*)(void*)) Systems::procAddressLib(lib, str);
    if(! fun_get_surface)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    return true;
}

int StoragePlugin::storeAction(void)
{
    if(threadInitialize && fun_store_action && data)
    {
        if(threadAction)
        {
            // wait action complete
            return 0;
        }
        else
        {
            threadAction = true;
            //DEBUG("thread action start: " << name);
	    if(thread.joinable())
		thread.join();
            thread = std::thread([this](){
                int err = this->fun_store_action(this->data);
                if(0 == err) DisplayScene::pushEvent(this->parent, ActionStoreComplete, nullptr);
                this->threadAction = false;
                this->threadResult = err;
            });
            return 0;
        }
    }

    return 1;
}

int StoragePlugin::setSurface(const Surface & sf)
{
    if(fun_set_surface && data)
    {
	if(threadInitialize)
        {
	    if(threadAction)
	    {
    	        if(thread.joinable())
                    thread.join();
                threadAction = false;
	    }

	    if(0 == threadResult)
	        return fun_set_surface(data, sf);
        }
    }

    return -1;
}

SurfaceLabel StoragePlugin::getSurfaceLabel(void)
{
    if(fun_get_surface && fun_get_label && data)
    {
	if(threadInitialize)
        {
	    if(threadAction)
	    {
    	        if(thread.joinable())
                    thread.join();
                threadAction = false;
	    }

	    if(0 == threadResult)
	        return SurfaceLabel(fun_get_surface(data), fun_get_label(data));
        }
    }

    return SurfaceLabel();
}

std::string StoragePlugin::findSignal(const std::string & str) const
{
    for(auto it = signals.begin(); it != signals.end(); ++it)
    {
	if((*it).size() > str.size())
	{
	    if(0 == (*it).compare(0, str.size(), str))
		return *it;
	}
	else
	if((*it).size() == str.size())
	{
	    if(0 == (*it).compare(str))
		return *it;
	}
    }

    return std::string();
}

/* SignalPlugin */
SignalPlugin::SignalPlugin(const PluginParams & params, Window & win)
    : BasePlugin(params, win), fun_action(nullptr), fun_stop_thread(nullptr)
{
    if(loadFunctions())
    {
	data = fun_init(config);
	if(data)
        {
            threadInitialize = true;

            // action mode: thread
            DEBUG("thread action start: " << name);
    	    thread = std::thread([this](){
                this->fun_action(this->data);
            });
        }
    }
}

SignalPlugin::~SignalPlugin()
{
    if(fun_stop_thread && data)
        fun_stop_thread(data);

    joinThread();
}

bool SignalPlugin::loadFunctions(void)
{
    if(! isValid())
	return false;

    std::string str;

    // fun_action
    str = std::string(type).append("_action");
    fun_action = (int (*)(void*)) Systems::procAddressLib(lib, str);
    if(! fun_action)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_stop_thread
    str = std::string(type).append("_stop_thread");
    fun_stop_thread = (void (*)(void*)) Systems::procAddressLib(lib, str);
    if(! fun_stop_thread)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    return true;
}
