/***************************************************************************
 *   Copyright (C) 2018 by FlyCapture team <public.irkutsk@gmail.com>      *
 *                                                                         *
 *   Part of the FlyCapture engine:                                        *
 *   https://github.com/AndreyBarmaley/fly-capture                         *
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

#include <algorithm>

#include "mainscreen.h"
#include "settings.h"
#include "plugins.h"

/* PluginParams */
PluginParams::PluginParams(const JsonObject & jo)
{
    name = jo.getString("name");
    file = jo.getString("file");

    if(file.size() && ! Systems::isFile(file))
    {
	ERROR("plugin not found: " << file);
	if(name.size()) file.clear();
    }

    // find name.so
    if(name.size() && file.empty())
    {
	StringList dirs;
	dirs << "plugins" << "libs";

	const StringList & shares = Systems::shareDirectories(Settings::programDomain());
	for(auto it = shares.begin(); it != shares.end(); ++it)
	    dirs << Systems::concatePath(*it, "plugins");

	for(auto it = dirs.begin(); it != dirs.end(); ++it)
	{
	    std::string filename = Systems::concatePath(*it, name).append(Systems::suffixLib());
	    if(Systems::isFile(filename))
	    {
		VERBOSE("plugin found: " << filename);
		file = filename;
		break;
	    }
	}
    }

    if(jo.isObject("config"))
    {
        config = *jo.getObject("config");
    }
    else
    if(jo.isString("config"))
    {
        JsonContentFile json(jo.getString("config"));

        if(json.isValid() && json.isObject())
            config = json.toObject();
    }
}

/* BasePlugin */
BasePlugin::BasePlugin(const PluginParams & params, Window & win) : PluginParams(params), lib(NULL), data(NULL),
    fun_get_name(NULL), fun_get_version(NULL), fun_init(NULL), fun_quit(NULL),
    threadInitialize(false), threadAction(false), threadExit(false), parent(& win), thread(NULL)
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
    if(thread)
    {
        int returnCode;
	threadExit = true;
        Tools::delay(200);
        DEBUG("wait thread: " << name);
        SDL_WaitThread(thread, & returnCode);
    }

    if(lib)
    {
        if(fun_quit && data)
	    fun_quit(data);
	Systems::closeLib(lib);
    }
}

bool BasePlugin::loadFunctions(void)
{
    if(! isValid())
	return false;

    std::string str;

    // fun_init
    str = std::string(name).append("_init");
    fun_init = (void* (*)(const JsonObject &)) Systems::procAddressLib(lib, str);
    if(! fun_init)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_quit
    str = std::string(name).append("_quit");
    fun_quit = (void (*)(void*)) Systems::procAddressLib(lib, str);
    if(! fun_quit)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_name
    str = std::string(name).append("_get_name");
    fun_get_name = (const char* (*)(void)) Systems::procAddressLib(lib, str);
    if(! fun_get_name)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_version
    str = std::string(name).append("_get_version");
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
    return fun_get_name ? fun_get_name() : NULL;
}

int BasePlugin::pluginVersion(void) const
{
    return fun_get_version ? fun_get_version() : 0;
}

/* CapturePlugin */
CapturePlugin::CapturePlugin(const PluginParams & params, Window & win) : BasePlugin(params, win),
    fun_frame_action(NULL), fun_get_surface(NULL)
{
    blue = generateBlueScreen(_("error"));

    if(loadFunctions())
    {
        blue = generateBlueScreen(_("initialize"));

	thread = SDL_CreateThread(runThreadInitialize, name.c_str(), this);
        SDL_DetachThread(thread);
        thread = NULL;
        DEBUG("detach thread: " << name);
    }
}

int CapturePlugin::runThreadInitialize(void* ptr)
{
    if(ptr)
    {
	CapturePlugin* capture = reinterpret_cast<CapturePlugin*>(ptr);

        if(capture->data)
            capture->fun_quit(capture->data);

        DEBUG("thread init start: " << capture->name);

	while(! capture->threadExit)
	{
	    if(NULL != (capture->data = capture->fun_init(capture->config)))
            {
		capture->threadInitialize = true;
                DEBUG("thread init complete: " << capture->name);
		return 0;
	    }

            // free res
	    capture->threadInitialize = false;
            ERROR("thread init broken: " << capture->name);

	    // delay 3 sec
	    Tools::delay(3000);
	}
    }

    return -1;
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
    str = std::string(name).append("_frame_action");
    fun_frame_action = (int (*)(void*)) Systems::procAddressLib(lib, str);
    if(! fun_frame_action)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_get_surface
    str = std::string(name).append("_get_surface");
    fun_get_surface = (const Surface & (*)(void*)) Systems::procAddressLib(lib, str);
    if(! fun_get_surface)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    return true;
}

int CapturePlugin::runThreadAction(void* ptr)
{
    if(ptr)
    {
	CapturePlugin* capture = reinterpret_cast<CapturePlugin*>(ptr);

        if(capture->data)
        {

            int err = capture->fun_frame_action(capture->data);
            DisplayScene::pushEvent(capture->parent, err ? ActionFrameError : ActionFrameComplete, NULL);
            capture->threadAction = false;
	    return 0;
        }
    }

    return -1;
}

int CapturePlugin::frameAction(void)
{
    if(fun_frame_action && data)
    {
        if(threadInitialize)
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
                thread = SDL_CreateThread(runThreadAction, name.c_str(), this);
                return 0;
            }
        }
    }

    DisplayScene::pushEvent(parent, ActionFrameComplete, NULL);
    return 1;
}

const Surface & CapturePlugin::getSurface(void)
{
    if(fun_get_surface && data)
    {
	if(threadInitialize)
        {
            int res = 0;

	    if(threadAction)
	    {
    	        SDL_WaitThread(thread, & res);
                threadAction = false;
	        thread = NULL;
	    }

	    if(0 == res)
	        return fun_get_surface(data);
        }
    }

    // return blue screen: no signal
    return blue;
}

/* StoragePlugin */
StoragePlugin::StoragePlugin(const PluginParams & params, Window & win) : BasePlugin(params, win),
    fun_store_action(NULL), fun_set_surface(NULL)
{
    signals = params.config.getStdList<std::string>("signals");
    std::transform(signals.begin(), signals.end(), signals.begin(), String::toLower);

    if(loadFunctions())
    {
	thread = SDL_CreateThread(runThreadInitialize, name.c_str(), this);
        SDL_DetachThread(thread);
        thread = NULL;
        DEBUG("detach thread: " << name);
    }
}

int StoragePlugin::runThreadInitialize(void* ptr)
{
    if(ptr)
    {
	StoragePlugin* storage = reinterpret_cast<StoragePlugin*>(ptr);

        if(storage->data)
            storage->fun_quit(storage->data);

        DEBUG("thread init start: " << storage->name);

	while(! storage->threadExit)
	{
	    if(NULL != (storage->data = storage->fun_init(storage->config)))
            {
		storage->threadInitialize = true;
                DEBUG("thread init complete: " << storage->name);
		return 0;
	    }

            // free res
	    storage->threadInitialize = false;
            ERROR("thread init broken: " << storage->name);

	    // delay 3 sec
	    Tools::delay(3000);
	}
    }

    return -1;
}

bool StoragePlugin::loadFunctions(void)
{
    if(! isValid())
	return false;

    std::string str;

    // fun_store_action
    str = std::string(name).append("_store_action");
    fun_store_action = (int (*)(void*)) Systems::procAddressLib(lib, str);
    if(! fun_store_action)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_set_surface
    str = std::string(name).append("_set_surface");
    fun_set_surface = (int (*)(void*, const Surface &)) Systems::procAddressLib(lib, str);
    if(! fun_set_surface)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_get_label
    str = std::string(name).append("_get_label");
    fun_get_label = (const std::string & (*)(void*)) Systems::procAddressLib(lib, str);
    if(! fun_get_label)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_get_surface
    str = std::string(name).append("_get_surface");
    fun_get_surface = (const Surface & (*)(void*)) Systems::procAddressLib(lib, str);
    if(! fun_get_surface)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    return true;
}

int StoragePlugin::runThreadAction(void* ptr)
{
    if(ptr)
    {
	StoragePlugin* storage = reinterpret_cast<StoragePlugin*>(ptr);

        if(storage->data)
        {
            int err = storage->fun_store_action(storage->data);
            if(0 == err) DisplayScene::pushEvent(storage->parent, ActionStoreComplete, NULL);
            storage->threadAction = false;
	    return 0;
        }
    }

    return -1;
}

int StoragePlugin::storeAction(void)
{
    if(fun_store_action && data)
    {
        if(threadInitialize)
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
                thread = SDL_CreateThread(runThreadAction, name.c_str(), this);
                return 0;
            }
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
            int res = 0;

	    if(threadAction)
	    {
    	        SDL_WaitThread(thread, & res);
                threadAction = false;
	        thread = NULL;
	    }

	    if(0 == res)
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
            int res = 0;

	    if(threadAction)
	    {
    	        SDL_WaitThread(thread, & res);
                threadAction = false;
	        thread = NULL;
	    }

	    if(0 == res)
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
SignalPlugin::SignalPlugin(const PluginParams & params, Window & win) : BasePlugin(params, win),
    tickval(0), isthread(false), fun_action(NULL)
{
    tickval = params.config.getInteger("tick", 0);
    isthread = params.config.getBoolean("thread", false);

    if(loadFunctions())
    {
	data = fun_init(config);
	if(data)
        {
            threadInitialize = true;

            // action mode: thread
            if(isthread)
            {
                DEBUG("thread action start: " << name);
    	        thread = SDL_CreateThread(fun_action, name.c_str(), data);
                SDL_DetachThread(thread);
                thread = NULL;
                DEBUG("detach thread: " << name);
            }
        }
    }
}

bool SignalPlugin::loadFunctions(void)
{
    if(! isValid())
	return false;

    std::string str;

    // fun_action
    str = std::string(name).append("_action");
    fun_action = (int (*)(void*)) Systems::procAddressLib(lib, str);
    if(! fun_action)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    return true;
}

bool SignalPlugin::isTick(u32 ms) const
{
    return 0 < tickval ? tt.check(ms, tickval) : false;
}

int SignalPlugin::signalAction(void)
{
    if(fun_action && data)
	return fun_action(data);

    return -1;
}
