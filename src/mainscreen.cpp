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

#include <sys/types.h>

#include <pwd.h>
#include <unistd.h>

#include <algorithm>

#include "settings.h"
#include "plugins.h"
#include "gallerywindow.h"
#include "videowindow.h"
#include "mainscreen.h"

MainScreen::MainScreen(const JsonObject & jo) : DisplayWindow(Color::Black), config(&jo), uid(0), pid(0)
{
    colorBack = jo.getString("display:background");
    auto tmp = new FontRenderTTF(jo.getString("font:file"), jo.getInteger("font:size", 12), jo.getBoolean("font:blend", false) ? SWE::RenderBlended : SWE::RenderSolid);

    if(tmp->isValid())
        frs.reset(tmp);
    else
	delete tmp;

    // load windows
    if(const JsonArray* ja = jo.getArray("windows"))
    {
	for(int index = 0; index < ja->size(); ++index)
	{
	    const JsonObject* jo2 = ja->getObject(index);
	    if(jo2)
	    {
		bool skip = jo2->getBoolean("window:skip", false);
		if(! skip) windows.emplace_back(new VideoWindow(WindowParams(*jo2, this), *this));
	    }
	}
    }

    // load signals
    for(auto & obj : getPluginsType("signal_"))
    {
	PluginParams params(*obj);
	DEBUG("load signal: " << params.name);

    	if(params.config.isValid())
    	{
	    signals.emplace_back(new SignalPlugin(params, *this));
	}
	else
	{
	    ERROR("json config invalid");
	}
    }

    if(jo.isObject("gallery"))
    {
	const JsonObject* jo2 = jo.getObject("gallery");
	Rect pos = JsonUnpack::rect(*jo2, "position");
	Color back = jo2->getString("background");
	gallery.reset(new GalleryWindow(pos.toPoint(), pos.toSize(), back, *this));
    }

    if(jo.isObject("datetime"))
    {
	const JsonObject* jo2 = jo.getObject("datetime");
	dateTimeFormat = jo2->getString("strftime:format", "%H:%M:%S");
	dateTimePos = JsonUnpack::point(*jo2, "position");
	if(dateTimeFormat.size())
	    dateTimeTexture = Display::renderText(fontRender(), String::strftime(dateTimeFormat), Color::Yellow);
    }

    pid = getpid();
    if(auto user = Systems::environment("USER"))
    {
	if(struct passwd* st = getpwnam(user))
	{
	    uid = st->pw_uid;
	    home.assign(st->pw_dir);
	}
    }

    setVisible(true);
}

MainScreen::~MainScreen()
{
    for(auto & ptr : windows)
	if(ptr) ptr->stopCapture();
}

const JsonObject* MainScreen::getPluginName(const std::string & name) const
{
    if(const JsonArray* ja = config->getArray("plugins"))
    {
        for(int index = 0; index < ja->size(); ++index)
        {
            if(const JsonObject* jo = ja->getObject(index))
            {
                if(name == jo->getString("name"))
		    return jo;
	    }
	}
    }

    return nullptr;
}

std::list<const JsonObject*> MainScreen::getPluginsType(const std::string & type) const
{
    std::list<const JsonObject*> res;

    if(const JsonArray* ja = config->getArray("plugins"))
    {
        for(int index = 0; index < ja->size(); ++index)
        {
            if(const JsonObject* jo = ja->getObject(index))
            {
                if(type == jo->getString("type").substr(0, type.length()))
		    res.push_back(jo);
	    }
	}
    }

    return res;
}

const FontRender & MainScreen::fontRender(void) const
{
    return frs ? *frs : static_cast<const FontRender &>(systemFont());
}

void MainScreen::renderWindow(void)
{
    renderClear(colorBack);
    if(dateTimeTexture.isValid()) renderTexture(dateTimeTexture, dateTimePos);
}

bool MainScreen::keyPressEvent(const KeySym & key)
{
    if(key.keycode() == Key::PAGEUP && gallery)
    {
	gallery->scrollUp(4);
    }
    else
    if(key.keycode() == Key::PAGEDOWN && gallery)
    {
	gallery->scrollDown(4);
    }
    if(key.keycode() == Key::ESCAPE)
    {
        TermGUI::MessageBox msg(Settings::programName(), _("Exit from program?"),
				    TermGUI::ButtonOk | TermGUI::ButtonCancel, fontRender(), this);
        if(TermGUI::ButtonOk == msg.exec())
        {
	    setVisible(false);
	    return true;
        }
    }
    else
    if(key.keycode() == Key::F4)
    {
	UnicodeList list;
	for(auto & win : windows)
	    list.emplace_back(win->label());

        TermGUI::ListBox box("Edit Window Params", list, 4, fontRender(), this);


        if(box.exec())
	{
	    auto ptr = std::find_if(windows.begin(), windows.end(), [&](auto & win){ return win->isLabel(box.result()); });
	    if(ptr != windows.end())
	    {
		Rect newPosition;
		if(showWindowPositionsDialog(ptr->get(), newPosition))
		{
		    (*ptr)->setSize(newPosition);
		    (*ptr)->setPosition(newPosition);
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
    TermGUI::InputBox input("Set Window Positions", 20, def, fontRender(), this);
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
    if(dateTimeTexture.isValid() &&
        ttDateTime.check(ms, 300))
    {
	dateTimeTexture = Display::renderText(fontRender(), String::strftime(dateTimeFormat), Color::Yellow);
    }
}

void MainScreen::addImageGallery(const Surface & sf, const std::string & label)
{
    if(gallery) gallery->addImage(sf, label);
}
