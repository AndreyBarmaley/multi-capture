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

#include "mainscreen.h"
#include "gallerywindow.h"

GalleryItem::GalleryItem(const Surface & sf, const std::string & label, Window & win) : WindowListItem(win), popupInfo(*this)
{
    Size sz(win.width(), win.width() * sf.height() / sf.width());

    thumbnail = Display::createTexture(sz - Size(2, 2));
    Display::renderSurface(sf, sf.rect(), thumbnail, thumbnail.rect());

    setSize(sz);
    setPosition(Point(0, 0));

    MainScreen* main = static_cast<MainScreen*>(win.parent());
    if(main)
    {
	const FontRender & frs = main->fontRender(); 

        Texture text = Display::renderText(frs, label, Color::Black);
	Texture tx = Display::renderRect(Color::MidnightBlue, Color::Wheat, text.size() + Size(6, 6));

	Display::renderTexture(text, text.rect(), tx, Rect(Point(3, 3), text.size()));
	popupInfo.setTexture(tx);
    }

    setVisible(true);
}

void GalleryItem::renderWindow(void)
{
    renderTexture(thumbnail, Point(1, 1));

    if(isSelected())
        renderRect(Color::Yellow, rect());
}

GalleryWindow::GalleryWindow(const Rect & pos, const Color & col, Window & win)
    : WindowListBox(pos, pos, pos.h > pos.w, win), backcol(col)
{
    setState(FlagOrderForeground);
    setVisible(true);
}

void GalleryWindow::renderWindow(void)
{
    renderClear(backcol);
    renderItems();
}

void GalleryWindow::addImage(const Surface & sf, const std::string & label)
{
    insertItem(new GalleryItem(sf, label, *this));
}
