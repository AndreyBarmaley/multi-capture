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

namespace Application
{
    std::string getPath(void);
}

const char* PluginValue::getName(int type)
{
    switch(type)
    {
        case PluginName:        return "pluginName";
        case PluginVersion:     return "pluginVersion";
        case PluginType:        return "pluginType";
        case CaptureSurface:    return "captureSurface";
        case SignalStopThread:  return "stopThread";
        case StorageLocation:   return "storageLocation";
        case StorageSurface:    return "storageSurface";
        case SessionId:         return "sessionId";
        case SessionName:       return "sessionName";
        case InitGui:           return "initGui";
        default: break;
    }

    return "unknown";
}

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
        DEBUG("find plugin: " << type);

	StringList dirs;

#ifdef MULTI_CAPTURE_PLUGINS
	dirs << Systems::concatePath(MULTI_CAPTURE_PLUGINS);
#endif
	dirs << Systems::concatePath(Application::getPath(), "plugins");

	for(auto & dir : Systems::shareDirectories(Settings::programDomain()))
	    dirs << Systems::concatePath(dir, "plugins");

	DEBUG("check plugin dirs: " << dirs.join(", "));

	for(auto & dir : dirs)
	{
	    std::string filename = Systems::concatePath(dir, type).append(Systems::suffixLib());
	    if(Systems::isFile(filename))
	    {
		VERBOSE("plugin found: " << filename << ", plugin name: " << name);
		file = filename;
		break;
	    }
	}
    }

    if(file.empty())
        ERROR("plugin not found: " << name << "(" << type << ")");

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

bool PluginParams::isCapture(void) const
{
    return type.substr(0, 8) == "capture_";
}

bool PluginParams::isStorage(void) const
{
    return type.substr(0, 8) == "storage_";
}

bool PluginParams::isSignal(void) const
{
    return type.substr(0, 7) == "signal_";
}

/* BasePlugin */
BasePlugin::BasePlugin(const PluginParams & params, Window & win) : PluginParams(params), lib(nullptr), data(nullptr),
    fun_set_value(nullptr), fun_get_value(nullptr), fun_init(nullptr), fun_quit(nullptr),
    threadInitialize(false), threadAction(false), threadExit(false), threadResult(PluginResult::DefaultOk), parent(& win)
{
    if(file.empty())
    {
        ERROR("plugin not found: " << name << ", type: " << type);
    }
    else
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
	{
	    fun_quit(data);
	    data = nullptr;
	}

	Systems::closeLib(lib);
    }
}

const PluginParams & BasePlugin::pluginParams(void) const
{
    return *this;
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

    // fun_set_value
    str = std::string(type).append("_set_value");
    fun_set_value = (bool (*)(void*, int, const void*)) Systems::procAddressLib(lib, str);
    if(! fun_set_value)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    // fun_get_value
    str = std::string(type).append("_get_value");
    fun_get_value = (bool (*)(void*, int, void*)) Systems::procAddressLib(lib, str);
    if(! fun_get_value)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    int ver = pluginVersion();
    if(PLUGIN_API != ver)
    {
        ERROR("incorrect plugin version: " << ver << ", current API: " << PLUGIN_API << ", plugin: " << pluginName());
        return false;
    }

    return true;
}

std::string BasePlugin::pluginName(void) const
{
    std::string val;
    fun_get_value(data, PluginValue::PluginName, & val);
    return val;
}

int BasePlugin::pluginVersion(void) const
{
    int val = 0;
    fun_get_value(data, PluginValue::PluginVersion, & val);
    return val;
}

int BasePlugin::pluginType(void) const
{
    int val = 0;
    if(fun_get_value(data, PluginValue::PluginType, & val))
        return val;
    return PluginType::Unknown;
}

/* CapturePlugin */
CapturePlugin::CapturePlugin(const PluginParams & params, Window & parent) : BasePlugin(params, parent),
    scaleImage(false), blueFormat(true)
{
    surf = generateBlueScreen(_("error"));
    scaleImage = params.config.getBoolean("scale");

    if(loadFunctions())
    {
        surf = generateBlueScreen(_("initialize"));

	thread = std::thread([this, win = & parent](){
            if(this->data)
	    {
                this->fun_quit(this->data);
		this->data = nullptr;
	    }

	    this->threadInitialize = false;
            DEBUG("thread init start: " << this->name);

	    while(! this->threadExit)
	    {
	        if(nullptr != (this->data = this->fun_init(this->config)))
                {
                    this->fun_set_value(this->data, PluginValue::InitGui, win);
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

bool CapturePlugin::isScaleImage(void) const
{
    return scaleImage;
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

    return true;
}

const Surface & CapturePlugin::getSurface(void)
{
    if(threadInitialize && fun_get_value && data)
    {
	if(threadAction)
	{
    	    if(thread.joinable())
                thread.join();
            threadAction = false;
	}

	if(PluginResult::DefaultOk == threadResult)
        {
            bool res = fun_get_value(data, PluginValue::CaptureSurface, & surf);
            if(res) blueFormat = false;
        }
    }

    return surf;
}

bool CapturePlugin::isBlue(const Surface & sf) const
{
    return blueFormat;
}

/* StoragePlugin */
StoragePlugin::StoragePlugin(const PluginParams & params, Window & parent) : BasePlugin(params, parent),
    tickPeriod(0), fun_store_action(nullptr)
{
    signals = params.config.getStdList<std::string>("signals");

    if(loadFunctions())
    {
	thread = std::thread([this, win = & parent](){
            if(this->data)
	    {
                this->fun_quit(this->data);
		this->data = nullptr;
	    }

            this->threadInitialize = false;
            DEBUG("thread init start: " << this->name);

	    while(! this->threadExit)
	    {
	        if(nullptr != (this->data = this->fun_init(this->config)))
                {
                    this->fun_set_value(this->data, PluginValue::InitGui, win);
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

    std::string signal = findSignal("tick:", false);
    if(signal.size())
    {
        tickPeriod = String::toInt(signal.substr(5, signal.size() - 5));
	if(tickPeriod < 0) tickPeriod = 0;
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
    fun_store_action = (int (*)(void*, const std::string &)) Systems::procAddressLib(lib, str);
    if(! fun_store_action)
    {
        ERROR("cannot load function: " << str);
        return false;
    }

    return true;
}

bool StoragePlugin::isTickEvent(u32 ms) const
{
    return 0 <= tickPeriod &&
            ttStorage.check(ms, tickPeriod);
}

void StoragePlugin::sessionReset(const SessionIdName & ss)
{
    fun_set_value(data, PluginValue::SessionId, & ss.id);
    fun_set_value(data, PluginValue::SessionName, & ss.name);
}

int StoragePlugin::storeAction(const std::string & signal)
{
    if(threadInitialize && fun_store_action && data)
    {
        if(threadAction && threadSignal == signal)
        {
            // skip fast double signal
            // wait action complete
            return 0;
        }

        threadAction = true;
        threadSignal = signal;

        //DEBUG("thread action start: " << name);
	if(thread.joinable())
	    thread.join();

        thread = std::thread([this, sig = signal](){
            int err = this->fun_store_action(this->data, sig);
            if(PluginResult::DefaultOk == err)
                DisplayScene::pushEvent(nullptr, ActionPushGallery, this);
            else
            if(PluginResult::Reset == err)
                DisplayScene::pushEvent(nullptr, ActionStorageReset, this);

            this->threadAction = false;
            this->threadResult = err;
        });

        return 0;
    }

    return 1;
}

void StoragePlugin::setSurface(const Surface & sf)
{
    if(threadInitialize && fun_set_value && data)
    {
	if(threadAction)
	{
    	    if(thread.joinable())
                thread.join();

            threadAction = false;
	}

	fun_set_value(data, PluginValue::StorageSurface, & sf);
    }
}

SurfaceLabel StoragePlugin::getSurfaceLabel(void)
{
    if(threadInitialize)
    {
	if(threadAction)
	{
    	    if(thread.joinable())
                thread.join();
            threadAction = false;
	}

        Surface surf;
        std::string label;

        if(fun_get_value(data, PluginValue::StorageLocation, & label) &&
            fun_get_value(data, PluginValue::StorageSurface, & surf))
	    return SurfaceLabel(surf, label);
    }

    return SurfaceLabel();
}

std::string StoragePlugin::findSignal(const std::string & str, bool strong) const
{
    if(strong)
    {
        auto it = std::find(signals.begin(), signals.end(), str);
        if(it != signals.end())
            return *it;
    }
    else
    {
        for(auto & sig: signals)
        {
	    if(0 == sig.compare(0, str.size(), str))
	        return sig;
        }
    }

    return std::string();
}

/* SignalPlugin */
SignalPlugin::SignalPlugin(const PluginParams & params, Window & parent)
    : BasePlugin(params, parent), fun_action(nullptr)
{
    if(loadFunctions())
    {
	thread = std::thread([this, win = &parent](){
            if(this->data)
	    {
                this->fun_quit(this->data);
		this->data = nullptr;
	    }

            this->threadInitialize = false;
            DEBUG("thread init start: " << this->name);

	    while(! this->threadExit)
	    {
	        if(nullptr != (this->data = this->fun_init(this->config)))
                {
                    this->fun_set_value(this->data, PluginValue::InitGui, win);
		    this->threadInitialize = true;
                    DEBUG("thread init complete: " << this->name);
		    break;
	        }

                // free res
                ERROR("thread init broken: " << this->name);
                std::this_thread::sleep_for(3000ms);
	    }

	    if(this->threadInitialize && ! this->threadExit)
                this->fun_action(this->data);
        });
    }
}

SignalPlugin::~SignalPlugin()
{
    if(fun_set_value && data)
        fun_set_value(data, PluginValue::SignalStopThread, nullptr);

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

    return true;
}
