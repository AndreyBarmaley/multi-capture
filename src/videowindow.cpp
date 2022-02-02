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
#include "videowindow.h"

using namespace std::chrono_literals;

/* WindowParams */
WindowParams::WindowParams(const JsonObject & jo, const MainScreen* main)
{
    labelName = jo.getString("label:name");
    labelColor = JsonUnpack::color(jo, "label:color", Color::Red);
    fillColor = JsonUnpack::color(jo, "window:fill", Color::Navy);
    position = JsonUnpack::rect(jo, "position");

    const JsonObject* jo2 = nullptr;

    jo2 = main->getPluginName(jo.getString("capture"));
    if(jo2) capture = PluginParams(*jo2);

    jo2 = main->getPluginName(jo.getString("storage"));
    if(jo2) storage = PluginParams(*jo2);
}

/* VideoWindow */
VideoWindow::VideoWindow(const WindowParams & params, Window & parent) : Window(params.position, params.position, & parent),
    WindowParams(params), capturePluginParamScale(false), capturePluginParamTick(0), signalPluginParamTick(0)
{
    if(labelName.empty())
	labelName = String::hex(Window::id());

    resetState(FlagModality);
    setState(FlagKeyHandle);

    // init capture plugin
    if(Systems::isFile(capture.file))
    {
	DEBUG(labelName << " plugin: " << capture.name);
	
	if(capture.config.isValid())
	{
	    capture.config.addArray("window:size", JsonPack::size( size() ));
	    capturePlugin.reset(new CapturePlugin(capture, *this));

	    capturePluginParamScale = capture.config.getBoolean("scale");
	    capturePluginParamTick = capture.config.getInteger("tick");

	    if(capturePluginParamTick < 0) capturePluginParamTick = 0;
	}
	else
	{
	    ERROR("json config invalid");
	}
    }
    else
    {
	ERROR("capture plugin not found: " << capture.file);
    }

    // init storage plugin
    if(storage.file.size())
    {
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
			    format = String::replace(format, "${user}", scr->getUserName());
			    format = String::replace(format, "${home}", scr->getHome());
			}
			format = String::replace(format, "${label}", label());
			storage.config.addString("format", format);
		    }
		}

		storagePlugin.reset(new StoragePlugin(storage, *this));

		std::string signal = storagePlugin->findSignal("tick:");
		if(signal.size())
		{
		    signalPluginParamTick = String::toInt(signal.substr(5, signal.size() - 5));
		}
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
	if(0 <= capturePluginParamTick &&
	    ttCapture.check(ms, capturePluginParamTick))
	{
	    capturePlugin->frameAction();
	}

	// internal signal: tick:timeout
	if(storagePlugin && capturePlugin->isInitComplete() && storagePlugin->isInitComplete())
	{
	    if(0 <= signalPluginParamTick &&
                ttStorage.check(ms, signalPluginParamTick))
	    {
		storagePlugin->setSurface(back);
		pushEventAction(ActionBackStore, this, nullptr);
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
	    capturePlugin.reset(new CapturePlugin(capture, *this));
            return true;

	case ActionBackSignal:
	    if(data && storagePlugin && capturePlugin->isInitComplete())
	    {
		const std::string* str = static_cast<const std::string*>(data);
		std::string signal = storagePlugin->findSignal(*str);

		if(signal.size())
		{
		    storagePlugin->setSurface(back);
		    storagePlugin->storeAction();
		}
	    }
	    // broadcast signal
	    return false;

	case ActionBackStore:
	    if(storagePlugin)
	    {
		storagePlugin->storeAction();
		return true;
	    }
	    break;

	case ActionStoreComplete:
	    if(storagePlugin)
	    {
		MainScreen* win = static_cast<MainScreen*>(parent());
		if(win)
		{
		    SurfaceLabel sl = storagePlugin->getSurfaceLabel();
		    std::string label = StringFormat("%1:/%2").arg(storagePlugin->pluginName()).arg(sl.label());
		    win->addImageGallery(sl.surface(), label);
		}
		return true;
	    }
	    break;

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

	    if(capturePluginParamScale)
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
    if(capturePlugin && capturePlugin->isInitComplete() &&
	storagePlugin && coord.isButtonLeft())
    {
	std::string signal = storagePlugin->findSignal("mouse:click");

	if(signal.size())
	{
	    DEBUG("receive signal: " << signal);
	    storagePlugin->setSurface(back);
	    pushEventAction(ActionBackStore, this, nullptr);
	    return true;
	}
    }

    return false;
}

bool VideoWindow::keyPressEvent(const KeySym & key)
{
    // internal signal: key:keyname
    if(capturePlugin && capturePlugin->isInitComplete() && storagePlugin)
    {
	std::string signal = storagePlugin->findSignal("key:");

	if(signal.size())
	{
	    std::string keystr = String::toUpper(signal.substr(4, signal.size() - 4));
	    if(keystr == key.keyname())
	    {
		DEBUG("receive signal: " << signal);
		storagePlugin->setSurface(back);
		pushEventAction(ActionBackStore, this, nullptr);
		return true;
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
