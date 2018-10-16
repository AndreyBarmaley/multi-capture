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

#include "settings.h"
#include "plugins.h"
#include "gallerywindow.h"
#include "videowindow.h"
#include "mainscreen.h"

MainScreen::MainScreen(const JsonObject & jo) : DisplayWindow(Color::Black), frs(NULL), gallery(NULL)
{
    colorBack = jo.getColor("display:background");

    frs = new FontRenderTTF(jo.getString("font:file"), jo.getInteger("font:size", 12), jo.getBoolean("font:blend", false));
    const JsonArray* ja = NULL;

    if(! frs->isValid())
    {
	delete frs;
	frs = & systemFont();
    }

    // load windows
    ja = jo.getArray("windows");
    if(ja)
    {
	for(int index = 0; index < ja->count(); ++index)
	{
	    const JsonObject* jo2 = ja->getObject(index);
	    if(jo2)
	    {
		bool skip = jo2->getBoolean("window:skip", false);
		if(! skip)
		    windows.push_back(new VideoWindow(WindowParams(*jo2), *this));
	    }
	}
    }

    // load signals
    ja = jo.getArray("signals");
    if(ja)
    {
	for(int index = 0; index < ja->count(); ++index)
	{
	    const JsonObject* jo2 = ja->getObject(index);
	    if(jo2)
	    {
		PluginParams params(*jo2);
		DEBUG("load signal: " << params.name);

    		if(params.config.isValid())
    		{
		    SignalPlugin* signal = new SignalPlugin(params);
		    if(signal->isThread()) signal->signalAction();

		    signals.push_back(signal);
		}
		else
		{
		    ERROR("json config invalid");
		}
	    }
	}
    }

    if(jo.isObject("gallery"))
    {
	const JsonObject* jo2 = jo.getObject("gallery");
	Rect pos = jo2->getRect("position");
	Color back = jo2->getColor("background");
	gallery = new GalleryWindow(pos, back, *this);
    }

    setVisible(true);
}

MainScreen::~MainScreen()
{
    if(gallery)
	delete gallery;

    if(frs != & systemFont())
	delete frs;

    for(auto it = signals.begin(); it != signals.end(); ++it)
	delete *it;

    for(auto it = windows.begin(); it != windows.end(); ++it)
	delete *it;
}

const FontRender & MainScreen::fontRender(void) const
{
    return frs ? *frs : systemFont();
}

void MainScreen::renderWindow(void)
{
    renderClear(colorBack);
}

bool MainScreen::keyPressEvent(int key)
{
    if(key == Key::ESCAPE)
    {
        TermGUI::MessageBox msg(Settings::programName(), _("Exit from my super program?"),
				    TermGUI::ButtonOk | TermGUI::ButtonCancel, fontRender(), *this);
        if(TermGUI::ButtonOk == msg.exec())
        {
	    setVisible(false);
	    return true;
        }
    }
    else
    if(key == Key::F4)
    {
	UCStringList list;
	for(auto it = windows.begin(); it != windows.end(); ++it)
	    list << UCString((*it)->name(), Color::PaleGreen);

        TermGUI::ListBox box("Edit Window Params", list, 4, fontRender(), *this);
        if(box.exec())
	{
	    auto it = std::find_if(windows.begin(), windows.end(), std::bind2nd(std::mem_fun(&VideoWindow::isName), box.result()));
	    if(it != windows.end())
	    {
		Rect newPosition;
		if(showWindowPositionsDialog(*it, newPosition))
		{
		    (*it)->setSize(newPosition);
		    (*it)->setPosition(newPosition);
		    DEBUG("set window position: " << newPosition.toString());
		    renderWindow();
		}
	    }
	}
    }

    return false;
}

bool MainScreen::showWindowPositionsDialog(const Window* win, Rect & res)
{
    if(! win) return false;

    std::string def = StringFormat("%1x%2+%3+%4").arg(win->width()).arg(win->height()).arg(win->position().x).arg(win->position().y);
    TermGUI::InputBox input("Set Window Positions", 20, def, fontRender(), *this);
    if(input.exec())
    {
	const std::string & str = input.result();

	auto it1 = str.begin();
	auto it2 = std::find(it1, str.end(), 'x');

	if(it2 == str.end())
	{
	    ERROR("incorrect format: WIDTHxHEIGHT+POSX+POSY");
	    return false;
	}

	int width = String::toInt(str.substr(std::distance(str.begin(), it1), std::distance(it1, it2)));
	it1 = ++it2;
	it2 = std::find(it1, str.end(), '+');

	if(it2 == str.end())
	{
	    ERROR("incorrect format: WIDTHxHEIGHT+POSX+POSY");
	    return false;
	}

	int height = String::toInt(str.substr(std::distance(str.begin(), it1), std::distance(it1, it2)));
	it1 = ++it2;
	it2 = std::find(it1, str.end(), '+');

	if(it2 == str.end())
	{
	    ERROR("incorrect format: WIDTHxHEIGHT+POSX+POSY");
	    return false;
	}

	int posx = String::toInt(str.substr(std::distance(str.begin(), it1), std::distance(it1, it2)));
	it1 = ++it2;
	it2 = str.end();

	int posy = String::toInt(str.substr(std::distance(str.begin(), it1), std::distance(it1, it2)));

	res = Rect(posx, posy, width, height);
	return true;
    }
    return false;
}

void MainScreen::tickEvent(u32 ms)
{
    for(auto it = signals.begin(); it != signals.end(); ++it)
    {
	if((*it)->isInit())
	{
	    if((*it)->isTick(ms)) (*it)->signalAction();
	}
	else
	{
	    // 500 ms
	    if(0 == (ms % 500)) (*it)->reInit();
	}
    }
}

void MainScreen::addImageGallery(const Surface & sf, const std::string & label)
{
    if(gallery) gallery->addImage(sf, label);
}
