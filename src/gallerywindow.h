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

#ifndef _CNA_GALLERY_WINDOW_
#define _CNA_GALLERY_WINDOW_

#include "engine.h"

class PopupInfo : public WindowToolTip
{
public:
    PopupInfo(Window & win) : WindowToolTip(300, & win)
    {
    }
};

class GalleryItem : public WindowListItem
{
    PopupInfo           popupInfo;
    Texture		thumbnail;

public:
    GalleryItem(const Surface &, const std::string &, Window &);

    void renderWindow(void);
};

class GalleryWindow : public WindowListBox
{
    Color		backcol;

public:
    GalleryWindow(const Rect &, const Color &, Window &);

    void renderWindow(void);
    void addImage(const Surface &, const std::string &);
};


#endif
