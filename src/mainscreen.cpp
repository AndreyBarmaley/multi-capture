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
#include <exception>

#include "settings.h"
#include "plugins.h"
#include "gallerywindow.h"
#include "videowindow.h"
#include "mainscreen.h"

MainScreen::MainScreen(const JsonObject & jo) : DisplayWindow(Color::Black), config(&jo), uid(0), pid(0), sid(0)
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
		try
                {
		    windows.emplace_back(std::make_unique<VideoWindow>(WindowParams(*jo2, this), *this));
                }
                catch(const std::invalid_argument & err)
                {
                    ERROR(err.what());
                }
	    }
	}
    }

    if(windows.empty())
        throw std::runtime_error("active windows not found");

    // load signals
    for(auto & obj : getPluginsType("signal_"))
    {
	PluginParams params(*obj);
	DEBUG("load signal plugin: " << params.name);

    	if(params.config.isValid())
    	{
	    signals.emplace_back(std::make_unique<SignalPlugin>(params, *this));
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
	gallery.reset(new GalleryWindow(pos.toPoint(), pos.toSize(), *jo2, *this));
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

    if(auto map = jo.getObject("keymap"))
    {
	keymap = map->toStdMap<std::string>();
    }

    setVisible(true);
}

MainScreen::~MainScreen()
{
    for(auto & ptr : windows)
	if(ptr) ptr->stopCapture();
}

const SignalPlugin* MainScreen::findSignalData(void* data) const
{
    auto it = std::find_if(signals.begin(), signals.end(), [=](auto & ptr){ return ptr->isData(data); });
    return it != signals.end() ? it->get() : nullptr;
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
    if(dateTimeTexture.isValid())
	renderTexture(dateTimeTexture, dateTimePos);
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

    if(! keymap.empty())
    {
	auto it = keymap.find(Key::toName(key.keycode()));
	if(it != keymap.end())
	{
	    actionSignalName(it->second, nullptr);
	    return true;
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
        dateTimeTexture.reset();
	dateTimeTexture = Display::renderText(fontRender(), String::strftime(dateTimeFormat), Color::Yellow);
    }
}

bool MainScreen::userEvent(int act, void* data)
{
    switch(act)
    {
        case ActionProgramExit:
        {
	    setVisible(false);
	    return true;
        }

        case ActionSessionReset:
            if(gallery) gallery->clear();
            if(auto ptr = static_cast<const SessionIdName*>(data))
            {
                sid = ptr->id;
                session = ptr->name;

                for(auto & win: windows)
                    win->actionSessionReset(*ptr);
            }
	    return true;

        case ActionPushGallery:
            if(auto plugin = static_cast<StoragePlugin*>(data))
            {
                SurfaceLabel sl = plugin->getSurfaceLabel();
                if(sl.surface().isValid())
                    addImageGallery(sl.surface(), sl.label());
            }
            return true;

        default: break;
    }

    return false;
}

void MainScreen::actionSignalName(const std::string & name, const VideoWindow* win)
{
    if(name == "shift:positions")
    {
	if(1 < windows.size())
	{
	    auto frontLabelPos = windows.front()->labelPosition();
	    auto frontArea = windows.front()->area();

	    auto it1 = windows.begin();
	    auto it2 = std::next(it1);

	    while(it2 != windows.end())
	    {
		const Rect & area = (*it2)->area();
		const Point & labelPos = (*it2)->labelPosition();

		(*it1)->setSize(area);
		(*it1)->setPosition(area);
		(*it1)->setLabelPosition(labelPos);

		it1 = it2;
		it2 = std::next(it1);
	    }

	    (*it1)->setSize(frontArea);
	    (*it1)->setPosition(frontArea);
	    (*it1)->setLabelPosition(frontLabelPos);

	    auto childs = DisplayScene::findChilds(*this);
	    if(1 < childs.size())
	    {
		auto ptr = childs.front();
		childs.pop_front();
		childs.push_back(ptr);

		for(auto win : childs)
		    DisplayScene::moveTopLayer(*win);
	    }

	    DisplayScene::setDirty(true);
	}
    }
}

void MainScreen::addImageGallery(const Surface & image, const std::string & location)
{
    if(gallery) gallery->addImage(image, location);
}

std::string MainScreen::formatString(const std::string & format) const
{
    auto str = String::replace(format, "${uid}", getUid());
    str = String::replace(str, "${pid}", getPid());
    str = String::replace(str, "${user}", getUserName());
    str = String::replace(str, "${home}", getHome());
    if(getSid())
    {
	str = String::replace(str, "${sid}", getSid());
	str = String::replace(str, "${session}", getSession());
    }
    return str;
}
