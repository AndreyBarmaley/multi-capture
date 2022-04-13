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

#include <thread>
#include <cstdlib>

#include "mainscreen.h"
#include "gallerywindow.h"

GalleryItem::GalleryItem(const Surface & image, const std::string & location, bool hidelabel, GalleryWindow & win) : ListWidgetItem(win), label(location)
{
    Size sz;

    if(win.width() < win.height())
        sz = Size(win.width(), win.width() * image.height() / image.width());
    else
        sz = Size(win.height() * image.width() / image.height(), win.height());

    thumbnail = Display::createTexture(sz - Size(2, 2));
    Display::renderSurface(image, image.rect(), thumbnail, thumbnail.rect());

    setSize(sz);
    setPosition(Point(0, 0));

    MainScreen* main = static_cast<MainScreen*>(win.parent());
    if(main)
    {
	renderToolTip(Systems::basename(label), main->fontRender(), Color::Black, Color::Wheat, Color::MidnightBlue);
	if(! hidelabel)
            Display::renderText(main->fontRender(), Systems::basename(label), Color::Yellow, thumbnail, Point(5, thumbnail.height() - 5), AlignLeft, AlignBottom);
    }

    setVisible(true);
}

const std::string & GalleryItem::getLabel(void) const
{
    return label;
}

void GalleryItem::renderWindow(void)
{
    renderTexture(thumbnail, Point(1, 1));

    if(isSelected())
        renderRect(Color::Yellow, rect());
}

GalleryWindow::GalleryWindow(const Point & pos, const Size & sz, const JsonObject & params, Window & win)
    : ListWidget(pos, sz, sz.h > sz.w, & win), hidelabel(false)
{
    backcol = params.getString("background");
    hidelabel = params.getBoolean("label:hide", false);
    setState(FlagLayoutForeground);
    action = params.getString("action:click", "xdg-open ${dir}");
    setVisible(true);
}

void GalleryWindow::renderWindow(void)
{
    renderClear(backcol);
    ListWidget::renderWindow();
}

void GalleryWindow::addImage(const Surface & image, const std::string & location)
{
    auto item = new GalleryItem(image, location, hidelabel, *this);
    addItem(item);
    setActiveItem(item);
}

void GalleryWindow::itemDoubleClicked(ListWidgetItem* ptr)
{
    auto item = static_cast<GalleryItem*>(ptr);
    if(item)
    {
        auto cmd = action;
        cmd = String::replace(cmd, "${dir}", Systems::dirname(item->getLabel()));
        cmd = String::replace(cmd, "${file}", item->getLabel());

        std::thread([=]{
            system(cmd.c_str());
        }).detach();
    }
}
