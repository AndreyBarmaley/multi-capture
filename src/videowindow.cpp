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
#include <exception>
#include <algorithm>

#include "mainscreen.h"
#include "videowindow.h"

using namespace std::chrono_literals;

/* WindowParams */
WindowParams::WindowParams(const JsonObject & jo, const MainScreen* main) : skip(false)
{
    labelName = jo.getString("label:name");
    labelFormat = jo.getString("label:format");
    skip = jo.getBoolean("window:skip", false);
    labelColor = JsonUnpack::color(jo, "label:color", Color::Red);
    fillColor = JsonUnpack::color(jo, "window:fill", Color::Navy);
    position = JsonUnpack::rect(jo, "position");
    labelPos = JsonUnpack::point(jo, "label:position", Point(10, 10));

    const JsonObject* jo2 = nullptr;

    // parse plugins
    if(const JsonArray* ja = jo.getArray("plugins"))
    {
        for(int ii = 0; ii < ja->size(); ++ii)
        {
            auto name = ja->getString(ii);

            if(nullptr != (jo2 = main->getPluginName(name)))
            {
                plugins.emplace_back(*jo2);
            }
            else
            {
                ERROR("plugin not found: " << name);
            }
        }
    }

    // old interface
    if(plugins.empty())
    {
        jo2 = main->getPluginName(jo.getString("capture"));
        if(jo2) plugins.emplace_back(*jo2);

        jo2 = main->getPluginName(jo.getString("storage"));
        if(jo2) plugins.emplace_back(*jo2);
    }
}

/* VideoWindow */
VideoWindow::VideoWindow(const WindowParams & params, Window & parent) : Window(params.position, params.position, & parent),
    WindowParams(params)
{
    if(labelName.empty())
	labelName = String::hex(Window::id());

    if(params.skip)
        throw std::invalid_argument("skip window");

    resetState(FlagModality);
    setState(FlagKeyHandle);

    // init capture plugin
    auto it = std::find_if(plugins.begin(), plugins.end(), [](auto & val){ return val.isCapture(); });
    if(it == plugins.end())
        throw std::invalid_argument("capture plugin not found");

    auto & captureParams = (*it);

    if(Systems::isFile(captureParams.file))
    {
	DEBUG("window label: " << labelName << ", capture plugin: " << captureParams.name);

	if(captureParams.config.isValid())
	{
	    captureParams.config.addArray("window:size", JsonPack::size( size() ));
	    capturePlugin.reset(new CapturePlugin(captureParams, *this));
	}
	else
	{
	    ERROR("json config invalid");
	}
    }
    else
    {
	ERROR("capture plugin not found: " << captureParams.file);
    }

    // init storage plugins
    for(auto & plugin : plugins)
    {
        if(! plugin.isStorage())
            continue;

        auto & storage = plugin;


        if(storage.file.empty())
            continue;

	if(Systems::isFile(storage.file))
	{
	    DEBUG("window label: " << labelName << ", storage plugin: " << storage.name);
	
	    if(storage.config.isValid())
	    {
		auto format = storage.config.getString("filename");
		if(format.empty())
		    format = storage.config.getString("format");

		if(! format.empty())
		{
		    if(const MainScreen* scr = dynamic_cast<const MainScreen*>(& parent))
                        format = scr->formatString(format);
		    format = String::replace(format, "${label}", label());
		    storage.config.addString("format", format);
		}
		storagePlugins.emplace_back(std::make_unique<StoragePlugin>(storage, *this));
	    }
	}
	else
	{
	    ERROR("storage plugin not found: " << storage.file);
	}
    }

    renderWindow();
    setVisible(true);
}

void VideoWindow::renderWindow(void)
{
    // capture
    if(capturePlugin)
    {
	if(back.isValid())
	{
	    Rect rt1 = back.rect();
	    Rect rt2 = rect();

	    if(capturePlugin->isScaleImage())
		Display::renderSurface(back, rt1, Display::texture(), rt2 + Window::position());
	    else
	    {
		Window::renderSurface(back, rt1, (rt2.toSize() - rt1.toSize()) / 2);
	    }
	}
	else
	{
	    renderClear(fillColor);
	}
    }
    else
    {
	renderClear(fillColor);
    }

    // label
    const MainScreen* scr = dynamic_cast<const MainScreen*>(parent());
    if(scr && ! labelColor.isTransparent())
    {
        std::string labelText = labelName;

        if(! labelFormat.empty())
        {
            labelText = scr->formatString(labelFormat);
            labelText = String::strftime(labelText);
            labelText = String::replace(labelText, "${label}", labelName);
        }

	renderText(scr->fontRender(), labelText, labelColor, labelPos);
    }
}

void VideoWindow::tickEvent(u32 ms)
{
    if(capturePlugin && capturePlugin->isInitComplete())
    {
        for(auto & plugin : storagePlugins)
        {
	    if(plugin && plugin->isInitComplete() && plugin->isTickEvent(ms))
	    {
                auto signalName = plugin->findSignal("tick:", false);
		if(signalName.empty()) continue;

                plugin->storeAction(signalName);
	    }
	}
    }
}

bool VideoWindow::userEvent(int act, void* data)
{
    switch(act)
    {
	case ActionFrameComplete:       return actionFrameComplete(data);
	case ActionCaptureReset:        return actionCaptureReset(data);
	case ActionStorageReset:        return actionStorageReset(data);
	case ActionStorageBack:         return actionStorageBack(data);
	case ActionSignalName:          return actionSignalName(data);
	default: break;
    }

    return false;
}

void VideoWindow::actionSessionReset(const SessionIdName & sn)
{
    for(auto & plugin : storagePlugins)
    	plugin->sessionReset(sn);
}

bool VideoWindow::actionFrameComplete(void* data)
{
    if(! capturePlugin->isData(data))
        return false;

    back = capturePlugin->getSurface();

    // store to all storage
    for(auto & plugin : storagePlugins)
	if(plugin && plugin->isInitComplete() && ! capturePlugin->isBlue(back))
            plugin->setSurface(back);

    renderWindow();
    return true;
}

bool VideoWindow::actionCaptureReset(void* data)
{
    if(! capturePlugin->isData(data))
        return false;

    // copy params
    auto params = capturePlugin->pluginParams();
    // unload dl
    capturePlugin.reset();
    std::this_thread::sleep_for(100ms);
    capturePlugin.reset(new CapturePlugin(params, *this));

    return true;
}

bool VideoWindow::actionStorageReset(void* data)
{
    auto it = std::find_if(storagePlugins.begin(), storagePlugins.end(),
                    [=](auto & ptr){ return ptr && ptr->isData(data); });
    if(it != storagePlugins.end())
    {
        auto params = (*it)->pluginParams();
        (*it).reset();
        std::this_thread::sleep_for(100ms);
        (*it).reset(new StoragePlugin(params, *this));

        return true;
    }

    // broadcast
    return false;
}

bool VideoWindow::actionStorageBack(void* data)
{
    for(auto & plugin : storagePlugins)
    {
        // data is StoragePlugin::data
        if(plugin && plugin->isInitComplete() && plugin->isData(data))
        {
            plugin->storeAction("ActionStorageBack");
            return true;
        }
    }

    // broadcast
    return false;
}

bool VideoWindow::actionSignalName(void* data)
{
    if(auto name = static_cast<std::string*>(data))
        actionSignalName(*name);

    // broadcast
    return false;
}

void VideoWindow::actionSignalName(const std::string & signalName2)
{
    if(capturePlugin && capturePlugin->isInitComplete())
    {
        // lookfor storage
        for(auto & plugin : storagePlugins)
        {
            if(plugin && plugin->isInitComplete())
            {
                auto signalName = plugin->findSignal(signalName2, true);
                if(signalName.empty()) continue;

                plugin->storeAction(signalName);
            }
        }
    }
}

bool VideoWindow::mousePressEvent(const ButtonEvent & coord)
{
    // internal signal: mouse:click
    if(capturePlugin && capturePlugin->isInitComplete() && coord.isButtonLeft())
    {
        for(auto & plugin : storagePlugins)
	{
	    if(plugin && plugin->isInitComplete())
	    {
                auto signalName = plugin->findSignal("mouse:click", false);
	        if(signalName.empty()) continue;

	        //DEBUG("receive signal: " << signalName);
                plugin->storeAction(signalName);
	        return true;
	    }
        }
    }

    return false;
}

bool VideoWindow::keyPressEvent(const KeySym & key)
{
    // internal signal: key:keyname
    if(capturePlugin && capturePlugin->isInitComplete())
    {
        for(auto & plugin : storagePlugins)
	{
	    if(plugin && plugin->isInitComplete())
	    {
	        auto signalName = plugin->findSignal("key:", false);
                if(signalName.empty()) continue;

                auto keyName = String::toUpper(signalName.substr(4, signalName.size() - 4));
	        if(keyName == key.keyname())
	        {
		    plugin->storeAction(signalName);
		    return true;
	        }
            }
	}
    }

    return false;
}

void VideoWindow::stopCapture(void)
{
    if(capturePlugin)
	capturePlugin->stopThread();
}
