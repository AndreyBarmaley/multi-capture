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

#ifndef _CNA_MAIN_SCREEN_
#define _CNA_MAIN_SCREEN_

#include <map>
#include <list>
#include <memory>

#include "plugins.h"
#include "videowindow.h"
#include "gallerywindow.h"

class SignalPlugin;
class VideoWindow;
class GalleryWindow;

class MainScreen : public DisplayWindow
{
    const JsonObject*   config;
    Color               colorBack;
    Texture             dateTimeTexture;
    std::string         dateTimeFormat;
    Point               dateTimePos;
    TickTrigger         ttDateTime;

    size_t		uid;
    size_t		pid;
    size_t              sid;
    std::string		username;
    std::string		home;
    std::string         session;

    std::list< std::unique_ptr<VideoWindow> > windows;
    std::list< std::unique_ptr<SignalPlugin> > signals;
    std::map< std::string, std::string > keymap;

    std::unique_ptr<FontRender> frs;
    std::unique_ptr<GalleryWindow> gallery;

    bool		showWindowPositionsDialog(const Window*, Rect &);

protected:
    bool		keyPressEvent(const KeySym &) override;
    void                tickEvent(u32 ms) override;
    bool                userEvent(int, void*) override;

public:
    MainScreen(const JsonObject &);
    ~MainScreen();

    void		renderWindow(void) override;
    const FontRender &  fontRender(void) const;
    void		addImageGallery(const Surface &, const std::string &);
    void                actionSignalName(const std::string &, const VideoWindow*);

    const JsonObject*            getPluginName(const std::string & name) const;
    std::list<const JsonObject*> getPluginsType(const std::string & type) const;
    const SignalPlugin*		 findSignalData(void*) const;

    size_t		getUid(void) const { return uid; }
    size_t		getPid(void) const { return pid; }
    size_t		getSid(void) const { return sid; }
    const std::string &	getUserName(void) const { return username; }
    const std::string &	getHome(void) const { return home; }
    const std::string &	getSession(void) const { return session; }

    std::string         formatString(const std::string &) const;
};

#endif
