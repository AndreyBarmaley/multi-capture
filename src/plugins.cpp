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

#include "engine.h"
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
BasePlugin::BasePlugin(const PluginParams & params) : PluginParams(params), lib(NULL), data(NULL),
    fun_get_name(NULL), fun_get_version(NULL), fun_init(NULL), fun_quit(NULL)
{
    lib = Systems::openLib(file);

    if(! lib)
	ERROR("cannot open library: " << name);

    loadFunctions();
}

BasePlugin::~BasePlugin()
{
    if(lib)
    {
	quit();
	Systems::closeLib(lib);
    }
}

void BasePlugin::quit(void) const
{
    if(fun_quit && data)
    {
	fun_quit(data);
	data = NULL;
    }
}

bool BasePlugin::loadFunctions(void)
{
    if(! isValid())
	return false;

    std::string str;

    // fun_init
    str = name;
    fun_init = (void* (*)(const JsonObject &)) Systems::procAddressLib(lib, str.append("_init"));
    if(! fun_init)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_quit
    str = name;
    fun_quit = (void (*)(void*)) Systems::procAddressLib(lib, str.append("_quit"));
    if(! fun_quit)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_name
    str = name;
    fun_get_name = (const char* (*)(void)) Systems::procAddressLib(lib, str.append("_get_name"));
    if(! fun_get_name)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_version
    str = name;
    fun_get_version = (int (*)(void)) Systems::procAddressLib(lib, str.append("_get_version"));
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

void BasePlugin::reInit(void)
{
    if(fun_init && ! data)
	data = fun_init(config);
}

/* CapturePlugin */
CapturePlugin::CapturePlugin(const PluginParams & params) : BasePlugin(params),
    isthread(false), thread(NULL), initComplete(false), fun_frame_action(NULL), fun_get_surface(NULL)
{
    isthread = params.config.getBoolean("thread", false);

    if(isthread)
    {
	generateBlueScreen(_("initialize..."));
	thread = SDL_CreateThread(initializeBackground, name.c_str(), this);
    }
    else
    {
	generateBlueScreen(_("signal out"));
	loadFunctions();
	if(fun_init) data = fun_init(params.config);
	initComplete = true;
    }
}

CapturePlugin::~CapturePlugin()
{
    if(thread)
    {
        int returnCode;
        DEBUG(name << ": wait thread...");
        SDL_WaitThread(thread, & returnCode);
    }
}

int CapturePlugin::initializeBackground(void* ptr)
{
    if(ptr)
    {
	CapturePlugin* capture = reinterpret_cast<CapturePlugin*>(ptr);

	capture->loadFunctions();

	if(capture->fun_init)
	{
	    capture->data = capture->fun_init(capture->config);
	    capture->initComplete = true;
	    capture->generateBlueScreen(_("signal out"));

	    return 0;
	}
    }

    return -1;
}

void CapturePlugin::generateBlueScreen(const std::string & label)
{
    Size winsz = JsonUnpack::size(config, "window:size");
    blue = Surface(winsz);
    blue.clear(Color::Blue);
    const FontRender & frs = FontRenderSystem();
    Size sfsz = frs.stringSize(label);
    frs.renderString(label, Color::White, (winsz - sfsz) / 2, blue);
}

bool CapturePlugin::loadFunctions(void)
{
    if(! isValid())
	return false;

    std::string str;

    // fun_action
    str = name;
    fun_frame_action = (int (*)(void*)) Systems::procAddressLib(lib, str.append("_frame_action"));
    if(! fun_frame_action)
    {
        ERROR("cannot load function: " << "frame_action");
        return false;
    }

    // fun_get_surface
    str = name;
    fun_get_surface = (const Surface & (*)(void*)) Systems::procAddressLib(lib, str.append("_get_surface"));
    if(! fun_get_surface)
    {
        ERROR("cannot load function: " << "get_surface");
        return false;
    }

    return true;
}

int CapturePlugin::frameAction(void)
{
    if(fun_frame_action && data)
    {
	if(isthread)
	{
	    if(! thread)
    		thread = SDL_CreateThread(fun_frame_action, name.c_str(), data);
	}
	else
	{
	    int res = fun_frame_action(data);

	    // error frame: free resource
	    if(0 > res) quit();
	}

	return 0;
    }
    return -1;
}

const Surface & CapturePlugin::getSurface(void) const
{
    if(fun_get_surface && data)
    {
        int res = 0;

	if(isthread)
	{
    	    SDL_WaitThread(thread, & res);
	    thread = NULL;
	}

	if(0 == res)
	    return fun_get_surface(data);

	// error frame: free resource
	quit();
    }

    // return blue screen: no signal
    return blue;
}

/* StoragePlugin */
StoragePlugin::StoragePlugin(const PluginParams & params) : BasePlugin(params),
    isthread(false), thread(NULL), fun_store_action(NULL), fun_set_surface(NULL)
{
    signals = params.config.getStringList("signals");
    std::transform(signals.begin(), signals.end(), signals.begin(), String::toLower);

    isthread = params.config.getBoolean("thread", false);
    loadFunctions();
    if(fun_init) data = fun_init(params.config);
}

StoragePlugin::~StoragePlugin()
{
    if(thread)
    {
        int returnCode;
        DEBUG(name << ": wait thread...");
        SDL_WaitThread(thread, & returnCode);
    }
}

bool StoragePlugin::loadFunctions(void)
{
    if(! isValid())
	return false;

    std::string str;

    // fun_store_action
    str = name;
    fun_store_action = (int (*)(void*)) Systems::procAddressLib(lib, str.append("_store_action"));
    if(! fun_store_action)
    {
        ERROR("cannot load function: " << "store_action");
        return false;
    }

    // fun_set_surface
    str = name;
    fun_set_surface = (int (*)(void*, const Surface &)) Systems::procAddressLib(lib, str.append("_set_surface"));
    if(! fun_set_surface)
    {
        ERROR("cannot load function: " << "set_surface");
        return false;
    }

    // fun_get_label
    str = name;
    fun_get_label = (const std::string & (*)(void*)) Systems::procAddressLib(lib, str.append("_get_label"));
    if(! fun_get_label)
    {
        ERROR("cannot load function: " << "get_label");
        return false;
    }

    // fun_get_surface
    str = name;
    fun_get_surface = (const Surface & (*)(void*)) Systems::procAddressLib(lib, str.append("_get_surface"));
    if(! fun_get_surface)
    {
        ERROR("cannot load function: " << "get_surface");
        return false;
    }

    return true;
}

int StoragePlugin::storeAction(void)
{
    if(fun_store_action && data)
    {
	if(isthread)
	{
	    if(! thread)
    		thread = SDL_CreateThread(fun_store_action, name.c_str(), data);
	}
	else
	{
	    return fun_store_action(data);
	}

	return 0;
    }

    return -1;
}

int StoragePlugin::setSurface(const Surface & sf)
{
    if(fun_set_surface && data)
    {
	if(isthread)
	{
    	    int returnCode;
    	    SDL_WaitThread(thread, & returnCode);
	    thread = NULL;
	}

	return fun_set_surface(data, sf);
    }

    return -1;
}

SurfaceLabel StoragePlugin::getSurfaceLabel(void) const
{
    if(fun_get_surface && fun_get_label && data)
    {
	if(isthread)
	{
	    int returnCode;
	    SDL_WaitThread(thread, & returnCode);
	    thread = NULL;
	}

	return SurfaceLabel(fun_get_surface(data), fun_get_label(data));
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
SignalPlugin::SignalPlugin(const PluginParams & params) : BasePlugin(params),
    isthread(false), thread(NULL), tickval(0), ticktmp(0), fun_action(NULL)
{
    isthread = params.config.getBoolean("thread", false);
    tickval = params.config.getInteger("tick", 0);

    loadFunctions();
    if(fun_init) data = fun_init(params.config);
}

SignalPlugin::~SignalPlugin()
{
}

bool SignalPlugin::loadFunctions(void)
{
    if(! isValid())
	return false;

    std::string str;

    // fun_action
    str = name;
    fun_action = (int (*)(void*)) Systems::procAddressLib(lib, str.append("_action"));
    if(! fun_action)
    {
        ERROR("cannot load function: " << "_action");
        return false;
    }

    return true;
}

bool SignalPlugin::isTick(u32 ms) const
{
    if(isthread || 0 >= tickval)
	return false;

    if(ticktmp + tickval < ms)
    {
	ticktmp = ms;
	return true;
    }

    return false;
}

int SignalPlugin::signalAction(void)
{
    if(fun_action && data)
    {
	if(isthread)
	{
	    if(! thread)
	    {
    		thread = SDL_CreateThread(fun_action, name.c_str(), data);
		SDL_DetachThread(thread);
	    }

	    return 0;
	}
	else
	{
	    return fun_action(data);
	}
    }

    return -1;
}
