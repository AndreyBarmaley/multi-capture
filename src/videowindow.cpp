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
#include "mainscreen.h"
#include "videowindow.h"

/* WindowParams */
WindowParams::WindowParams(const JsonObject & jo)
{
    labelName = jo.getString("label:name");
    labelColor = jo.getColor("label:color", Color::Red);
    fillColor = jo.getColor("window:fill", Color::Navy);
    position = jo.getRect("position");

    const JsonObject* jo2 = NULL;

    jo2 = jo.getObject("capture");
    if(jo2) capture = PluginParams(*jo2);

    jo2 = jo.getObject("storage");
    if(jo2) storage = PluginParams(*jo2);
}

/* VideoWindow */
VideoWindow::VideoWindow(const WindowParams & params, Window & parent) : Window(params.position, params.position, & parent),
    WindowParams(params), capturePlugin(NULL), storagePlugin(NULL),
    capturePluginParamScale(false), capturePluginParamTick(0), capturePluginParamTickSave(0), signalPluginParamTickSave(0)
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
	    capture.config.addString("window:parent", String::pointer(this));
	    capture.config.addSize("window:size", size());

	    capturePlugin = new CapturePlugin(capture);

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
		storage.config.addString("window:parent", String::pointer(this));
		storagePlugin = new StoragePlugin(storage);
	    }
	    else
	    {
		ERROR("json config invalid");
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

VideoWindow::~VideoWindow()
{
    if(capturePlugin) delete capturePlugin;
    if(storagePlugin) delete storagePlugin;
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
    if(capturePlugin && capturePlugin->isInitComplete())
    {
	if(capturePlugin->isInit())
	{
	    if(0 == capturePluginParamTickSave || (0 < capturePluginParamTick && capturePluginParamTickSave + capturePluginParamTick < ms))
	    {
		if(0 == capturePlugin->frameAction())
		    pushEventAction(ActionRenderWindow, this, NULL);

		// && capturePlugin->isInitComplete()
		capturePluginParamTickSave = ms;
	    }
	}
	else
	{
    	    // 500 ms
	    if(0 == (ms % 500)) capturePlugin->reInit();
	}

	// internal signal: tick:timeout
	if(storagePlugin && storagePlugin->isInit())
	{
	    std::string signal = storagePlugin->findSignal("tick:");

	    if(signal.size())
	    {
		int tick = String::toInt(signal.substr(5, signal.size() - 5));

		if(0 < tick && signalPluginParamTickSave + tick < ms)
		{
		    DEBUG("receive signal: " << signal);
		    storagePlugin->setSurface(back);
		    pushEventAction(ActionBackStore, this, NULL);
		    signalPluginParamTickSave = ms;
		}
	    }
	}
	else
	if(storagePlugin && ! storagePlugin->isInit())
	{
    	    // 500 ms
    	    if(0 == (ms % 500)) storagePlugin->reInit();
	}
    }
}

bool VideoWindow::userEvent(int act, void* data)
{
    if(! capturePlugin || !capturePlugin->isInitComplete())
	return false;

    switch(act)
    {
	case ActionRenderWindow:
	    renderWindow();
	    return true;

	case ActionBackSignal:
	    if(data)
	    {
		const std::string* str = static_cast<const std::string*>(data);
		if(storagePlugin && str)
		{
		    std::string signal = storagePlugin->findSignal(*str);
		    if(signal.size())
		    {
			storagePlugin->setSurface(back);
			storagePlugin->storeAction();
		    }
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
	if(0 == capturePluginParamTickSave || ! back.isValid() ||
	    Tools::ticks() - capturePluginParamTickSave >= capturePluginParamTick)
	{
	    u32 tick1 = Tools::ticks();
	    back = capturePlugin->getSurface();
	    u32 delay = Tools::ticks() - tick1;

	    if(0 < capturePluginParamTick && delay > capturePluginParamTick)
		DEBUG(labelName << ": sloow process, maybe need increase tick");
	}
	else
	{
	    // DEBUG(labelName << ": skip render, because tick present, wait complete it");
	}

	if(back.isValid())
	{
	    Rect rt1 = back.rect();
	    Rect rt2 = rect();

	    if(capturePluginParamScale)
		Window::renderSurface(back, rt1, rt2);
	    else
	    {
		if(rt1.w < rt2.w)
		    Window::renderSurface(back, rt1, rt1);
		else
		    Window::renderSurface(back, rt2, rt2);
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
	    pushEventAction(ActionBackStore, this, NULL);
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
		pushEventAction(ActionBackStore, this, NULL);
		return true;
	    }
	}
    }

    return false;
}
