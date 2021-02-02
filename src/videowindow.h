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

#ifndef _CNA_VIDEO_WINDOW_
#define _CNA_VIDEO_WINDOW_

#include <memory>

#include "settings.h"
#include "plugins.h"

struct WindowParams
{
    std::string		labelName;
    Color		labelColor;
    Color		fillColor;
    Rect		position;
    PluginParams	capture;
    PluginParams	storage;

    WindowParams() {}
    WindowParams(const JsonObject &);
};

class VideoWindow : public Window, protected WindowParams
{
    std::unique_ptr<CapturePlugin> capturePlugin;
    std::unique_ptr<StoragePlugin> storagePlugin;

    Surface		back;
    TickTrigger		ttStorage, ttCapture;

    bool		capturePluginParamScale;
    int			capturePluginParamTick;
    int			signalPluginParamTick;

protected:
    void		tickEvent(u32 ms) override;
    bool		userEvent(int, void*) override;
    bool		keyPressEvent(const KeySym &) override;
    bool		mousePressEvent(const ButtonEvent &) override;

    void		renderSurface(void);

public:
    VideoWindow(const WindowParams &, Window & parent);

    const std::string &	name(void) const { return labelName; }
    bool		isName(const std::string & str) const { return 0 == labelName.compare(str); }

    void		renderWindow(void) override;
};

#endif
