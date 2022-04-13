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

#include <memory>

#include "../../settings.h"

#ifdef __cplusplus
extern "C" {
#endif

const int signal_gui_button_version = PLUGIN_API;

struct signal_gui_button_t
{
    int         debug;
    std::string buttonClick;
    std::unique_ptr<Window> win;
    Point       position;
    Surface     press;
    Surface     release;

    signal_gui_button_t() : debug(0)
    {
    }

    ~signal_gui_button_t()
    {
	clear();
    }

    void clear(void)
    {
        debug = 0;
        buttonClick = "pushButton";
    }
};

class SignalButton : public TextureButton
{
    const signal_gui_button_t* ptr;

public:
    SignalButton(const signal_gui_button_t & st, Window* win) : TextureButton(st.position, st.release, st.press, 0, win), ptr(& st)
    {
        signalSubscribe(*this, Signal::ButtonClicked);
        setVisible(true);
    }

    void signalReceive(int sig, const SignalMember* sm) override
    {
        if(sig == Signal::ButtonClicked && sm == this)
            DisplayScene::pushEvent(nullptr, ActionSignalName, (void*) & ptr->buttonClick);
    }
};

void* signal_gui_button_init(const JsonObject & config)
{
    VERBOSE("version: " << signal_gui_button_version);

    auto ptr = std::make_unique<signal_gui_button_t>();

    ptr->debug = config.getInteger("debug", 0);
    ptr->buttonClick = config.getString("signal:button", "pushButton");
    ptr->position = JsonUnpack::point(config, "position");
    auto imagePress = config.getString("image:press");
    auto imageRelease = config.getString("image:release");

    if(! ptr->buttonClick.empty())
        DEBUG("params: " << "signal:button = " << ptr->buttonClick);

    if(! imagePress.empty())
    {
        DEBUG("params: " << "image:press = " << imagePress);

        if(Systems::isFile(imagePress))
            ptr->press = Surface(imagePress);
        else
            ERROR("image not found: " << imagePress);
    }

    if(! imageRelease.empty())
    {
        DEBUG("params: " << "image:release = " << imageRelease);

        if(Systems::isFile(imageRelease))
            ptr->release = Surface(imageRelease);
        else
            ERROR("image not found: " << imageRelease);
    }

    return ptr.release();
}

void signal_gui_button_quit(void* ptr)
{
    signal_gui_button_t* st = static_cast<signal_gui_button_t*>(ptr);
    if(st->debug) DEBUG("version: " << signal_gui_button_version);

    delete st;
}

int signal_gui_button_action(void* ptr)
{
    signal_gui_button_t* st = static_cast<signal_gui_button_t*>(ptr);
    if(3 < st->debug) DEBUG("version: " << signal_gui_button_version);

    return PluginResult::DefaultOk;
}

bool signal_gui_button_get_value(void* ptr, int type, void* val)
{
    switch(type)
    {
        case PluginValue::PluginName:
            if(auto res = static_cast<std::string*>(val))
            {
                res->assign("signal_gui_button");
                return true;
            }
            break;
    
        case PluginValue::PluginVersion:
            if(auto res = static_cast<int*>(val))
            {
                *res = signal_gui_button_version;
                return true;
            }
            break;

        case PluginValue::PluginType:
            if(auto res = static_cast<int*>(val))
            {
                *res = PluginType::Signal;
                return true;
            }
            break;

        default: break;
    }       

    if(ptr)
    {
        signal_gui_button_t* st = static_cast<signal_gui_button_t*>(ptr);
        if(4 < st->debug)
            DEBUG("version: " << signal_gui_button_version << ", type: " << type);

        switch(type)
        {
            default: break;
        }
    }

    return false;
}

bool signal_gui_button_set_value(void* ptr, int type, const void* val)
{
    signal_gui_button_t* st = static_cast<signal_gui_button_t*>(ptr);
    if(4 < st->debug)
        DEBUG("version: " << signal_gui_button_version << ", type: " << type);

    switch(type)
    {
        case PluginValue::SignalStopThread:
            break;

        case PluginValue::InitGui:
            if(auto res = static_cast<const Window*>(val))
            {
                st->win.reset(new SignalButton(*st, const_cast<Window*>(res)));
            }
            break;

        default: break;
    }

    return false;
}

#ifdef __cplusplus
}
#endif
