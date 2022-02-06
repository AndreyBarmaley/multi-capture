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
    skip = jo.getBoolean("window:skip", false);
    labelColor = JsonUnpack::color(jo, "label:color", Color::Red);
    fillColor = JsonUnpack::color(jo, "window:fill", Color::Navy);
    position = JsonUnpack::rect(jo, "position");

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
    WindowParams(params), captureParams(nullptr)
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

    captureParams = & (*it);

    if(Systems::isFile(captureParams->file))
    {
	DEBUG(labelName << " plugin: " << captureParams->name);

	if(captureParams->config.isValid())
	{
	    captureParams->config.addArray("window:size", JsonPack::size( size() ));
	    capturePlugin.reset(new CapturePlugin(*captureParams, *this));
	}
	else
	{
	    ERROR("json config invalid");
	}
    }
    else
    {
	ERROR("capture plugin not found: " << captureParams->file);
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
	    DEBUG(labelName << " plugin: " << storage.name);
	
	    if(storage.config.isValid())
	    {
		if(storage.config.isString("format"))
		{
		    auto format = storage.config.getString("format");
		    if(! format.empty())
		    {
			if(const MainScreen* scr = dynamic_cast<const MainScreen*>(& parent))
			{
			    format = String::replace(format, "${uid}", scr->getUid());
			    format = String::replace(format, "${pid}", scr->getPid());
			    format = String::replace(format, "${sid}", scr->getSid());
			    format = String::replace(format, "${user}", scr->getUserName());
			    format = String::replace(format, "${home}", scr->getHome());
			    format = String::replace(format, "${session}", scr->getSession());
			}
			format = String::replace(format, "${label}", label());
			storage.config.addString("format", format);
		    }
		}

		storagePlugins.emplace_back(new StoragePlugin(storage, *this));
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
    renderSurface();

    const MainScreen* scr = dynamic_cast<const MainScreen*>(parent());
    if(scr && ! labelColor.isTransparent())
	renderText(scr->fontRender(), labelName, labelColor, Point(10, 10));
}

void VideoWindow::tickEvent(u32 ms)
{
    if(capturePlugin)
    {
	if(capturePlugin->isTickEvent(ms))
	{
	    capturePlugin->frameAction();
	}

	// internal signal: tick:timeout
	if(capturePlugin->isInitComplete())
	{
            for(auto & plugin : storagePlugins)
            {
	        if(plugin && plugin->isInitComplete() && plugin->isTickEvent(ms))
	        {
		    plugin->setSurface(back);
		    plugin->storeAction();
                }
	    }
	}
    }
}

bool VideoWindow::userEvent(int act, void* data)
{
    switch(act)
    {
	case ActionFrameComplete:
            back = capturePlugin->getSurface();
	    renderWindow();
	    return true;

	case ActionPluginReset:
	    // unload dl
	    capturePlugin.reset();
	    std::this_thread::sleep_for(100ms);
	    if(captureParams)
                capturePlugin.reset(new CapturePlugin(*captureParams, *this));
            return true;

	case ActionSignalBack:
	    // data is SignalPlugin::data
	    if(data && capturePlugin->isInitComplete())
	    {
		const MainScreen* scr = dynamic_cast<const MainScreen*>(parent());
		const SignalPlugin* signalPlugin = scr ? scr->findSignalData(data) : nullptr;

		if(signalPlugin)
		{
                    for(auto & plugin : storagePlugins)
                        if(plugin) plugin->signalBackAction(signalPlugin->signalName(), back);
		}
                else
		{
		    ERROR("ActionSignalBack: signal plugin not found, data: " << String::pointer(data));
		}
	    }
	    // broadcast signal
	    return false;

	default: break;
    }

    return false;
}

void VideoWindow::renderSurface(void)
{
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
}

bool VideoWindow::mousePressEvent(const ButtonEvent & coord)
{
    // internal signal: mouse:click
    if(capturePlugin && capturePlugin->isInitComplete() && coord.isButtonLeft())
    {
        for(auto & plugin : storagePlugins)
	{
            std::string signal = plugin ? plugin->findSignal("mouse:click") : "";

	    if(signal.size())
	    {
	        DEBUG("receive signal: " << signal);
	        plugin->setSurface(back);
	        plugin->storeAction();
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
	    std::string signal = plugin ? plugin->findSignal("key:") : "";

	    if(signal.size())
	    {
	        std::string keystr = String::toUpper(signal.substr(4, signal.size() - 4));
	        if(keystr == key.keyname())
	        {
		    DEBUG("receive signal: " << signal);
		    plugin->setSurface(back);
		    plugin->storeAction();
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
