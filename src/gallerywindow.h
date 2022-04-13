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

#ifndef _CNA_GALLERY_WINDOW_
#define _CNA_GALLERY_WINDOW_

#include "settings.h"
class GalleryWindow;

class GalleryItem : public ListWidgetItem
{
    Texture	thumbnail;
    std::string label;

public:
    GalleryItem(const Surface &, const std::string &, bool, GalleryWindow &);

    void	renderWindow(void) override;
    const std::string & getLabel(void) const;
};

class GalleryWindow : public ListWidget
{
    std::string action;
    Color	backcol;
    bool        hidelabel;

public:
    GalleryWindow(const Point &, const Size &, const JsonObject &, Window &);

    void 	renderWindow(void) override;
    void        itemDoubleClicked(ListWidgetItem*) override;
    void 	addImage(const Surface &, const std::string &);
};

#endif
