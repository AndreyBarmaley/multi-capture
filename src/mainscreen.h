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

#ifndef _CNA_MAIN_SCREEN_
#define _CNA_MAIN_SCREEN_

#include "settings.h"

class SignalPlugin;
class VideoWindow;
class GalleryWindow;

class MainScreen : public DisplayWindow
{
    Color                    colorBack;
    const FontRender*        frs;
    std::list<VideoWindow*>  windows;
    std::list<SignalPlugin*> signals;
    GalleryWindow*           gallery;

    bool		showWindowPositionsDialog(const Window*, Rect &);

protected:
    bool		keyPressEvent(const KeySym &) override;
    void                tickEvent(u32 ms) override;

public:
    MainScreen(const JsonObject &);
    ~MainScreen();

    void		renderWindow(void) override;
    const FontRender &  fontRender(void) const;
    void		addImageGallery(const Surface &, const std::string &);
};

#endif
