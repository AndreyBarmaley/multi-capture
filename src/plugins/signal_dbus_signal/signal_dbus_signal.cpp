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

#include <list>
#include <atomic>
#include <memory>
#include <cstring>

#include "../../settings.h"
#include "dbus/dbus.h"

#ifdef __cplusplus
extern "C" {
#endif

const int signal_dbus_signal_version = 20220412;

struct signal_dbus_signal_t
{
    std::atomic<bool> is_thread;

    int         debug;
    int         delay;

    std::string dbus_name;
    std::string dbus_object;
    std::string dbus_interface;
    StringList  dbus_signals;
    std::string dbus_autostart;
    DBusConnection* dbus_conn;

    signal_dbus_signal_t() : is_thread(true), debug(0), delay(100), dbus_conn(nullptr) {}
    ~signal_dbus_signal_t()
    {
	clear();
    }

    bool init(bool isSystem, bool ownerService)
    {
	// dbus init
	DBusError dbus_err;
	dbus_error_init(&dbus_err);

	// connect to the bus and check for errors
	dbus_conn = dbus_bus_get((isSystem ? DBUS_BUS_SYSTEM : DBUS_BUS_SESSION), &dbus_err);
        if(dbus_error_is_set(&dbus_err))
	{
	    ERROR("dbus_bus_get: " << dbus_err.message);
	    dbus_error_free(&dbus_err);
	    return false;
	}

	if(! dbus_conn)
    	    return false;

	if(ownerService)
	{
	    int ret = dbus_bus_request_name(dbus_conn, dbus_name.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING , &dbus_err);
	    if(dbus_error_is_set(&dbus_err))
	    {
		ERROR("dbus_bus_request_name: " << dbus_err.message);
		dbus_error_free(&dbus_err);
    		return false;
	    }

	    if(debug)
	    {
		VERBOSE("dbus_bus_request_name: ret code: " << ret);
	    }

	    if(0 > ret)
    		return false;
	}

	// add a rule for which messages we want to see
	std::string matchForm = StringFormat("type='%1',interface='%2'").arg("signal").arg(dbus_interface);
	DEBUG("math form: " << matchForm);

	dbus_bus_add_match(dbus_conn, matchForm.c_str(), &dbus_err);
	if(dbus_error_is_set(&dbus_err))
	{
	    ERROR("dbus_bus_add_match: " << dbus_err.message);
	    dbus_error_free(&dbus_err);
    	    return false;
	}

	dbus_connection_flush(dbus_conn);

	return true;
    }

    int dbusSignalAutostart(void)
    {
	// create a new method call and check for errors
        std::unique_ptr<DBusMessage, decltype(dbus_message_unref)*> msgCall {
            dbus_message_new_method_call(dbus_name.c_str(), dbus_object.c_str(), dbus_interface.c_str(), dbus_autostart.c_str()),
            dbus_message_unref };

	if(! msgCall)
	{
	    ERROR("dbus_message_new_method_call: failed");
	    return 0;
	}

	// send message and get a handle for a reply
	DBusPendingCall* pending = nullptr;
	if(!dbus_connection_send_with_reply(dbus_conn, msgCall.get(), &pending, -1  /* -1 is default timeout */))
	{
	    ERROR("dbus_connection_send_with_reply: failed");
	    return 0;
	}

	if(! pending)
	{
	    ERROR("pending call is null");
	    return 0;
	}

	dbus_connection_flush(dbus_conn);
	if(debug) VERBOSE("request sent");

	// block until we recieve a reply
	dbus_pending_call_block(pending);

	// get the reply message
        std::unique_ptr<DBusMessage, decltype(dbus_message_unref)*> msgReply {
	    dbus_pending_call_steal_reply(pending),
            dbus_message_unref };

	if(! msgReply)
	{
	    ERROR("dbus_pending_call_steal_reply: failed");
	    return 0;
	}

	// free the pending message handle
	dbus_pending_call_unref(pending);
        return 1;
    }

    void clear(void)
    {
        is_thread = true;
        debug = 0;
        delay = 100;
	dbus_name.clear();
	dbus_object.clear();
	dbus_interface.clear();
	dbus_signals.clear();
	dbus_conn = nullptr;
    }
};

void* signal_dbus_signal_init(const JsonObject & config)
{
    VERBOSE("version: " << signal_dbus_signal_version);

    auto ptr = std::make_unique<signal_dbus_signal_t>();

    ptr->debug = config.getInteger("debug", 0);
    ptr->delay = config.getInteger("delay", 100);
    bool dbusIsSystem = config.getBoolean("dbus:system", false);
 
    ptr->dbus_name = config.getString("dbus:name", "org.test.dbus");
    ptr->dbus_object = config.getString("dbus:object", "/org/test/dbus/object");
    ptr->dbus_interface = config.getString("dbus:interface", "org.test.dbus.interface");
    ptr->dbus_autostart = config.getString("dbus:autostart");
    ptr->dbus_signals = config.getStdList<std::string>("dbus:signals");
    ptr->dbus_signals.emplace_back(config.getString("dbus:signal", "test_signal"));

    if(! ptr->init(dbusIsSystem, ptr->dbus_autostart.empty()))
	return nullptr;

    DEBUG("params: " << "delay = " << ptr->delay);
    DEBUG("params: " << "dbus:system" << " = " << String::Bool(dbusIsSystem));
    DEBUG("params: " << "dbus:name" << " = " << ptr->dbus_name);
    DEBUG("params: " << "dbus:object" << " = " << ptr->dbus_object);
    DEBUG("params: " << "dbus:interface" << " = " << ptr->dbus_interface);
    DEBUG("params: " << "dbus:signals" << " = " << "[ " << ptr->dbus_signals.join(", ") << " ]");

    if(ptr->dbus_autostart.size())
	ptr->dbusSignalAutostart();

    return ptr.release();
}

void signal_dbus_signal_quit(void* ptr)
{
    signal_dbus_signal_t* st = static_cast<signal_dbus_signal_t*>(ptr);
    if(st->debug) DEBUG("version: " << signal_dbus_signal_version);

    delete st;
}

int signal_dbus_signal_action(void* ptr)
{
    signal_dbus_signal_t* st = static_cast<signal_dbus_signal_t*>(ptr);
    if(3 < st->debug) DEBUG("version: " << signal_dbus_signal_version);

    // loop listening for signals being emmitted
    while(st->is_thread)
    {
	// non blocking read of the next available message
	dbus_connection_read_write(st->dbus_conn, 0);

        std::unique_ptr<DBusMessage, decltype(dbus_message_unref)*> msg {
            dbus_connection_pop_message(st->dbus_conn),
            dbus_message_unref };

	// loop again if we haven't read a message
	if(! msg)
	{
	    Tools::delay(st->delay);
	    continue;
	}

        for(auto & dbus_signal : st->dbus_signals)
	{
            // check if the message is a signal from the correct interface and with the correct name
	    if(dbus_message_is_signal(msg.get(), st->dbus_interface.c_str(), dbus_signal.c_str()))
	    {
	        if(2 < st->debug) DEBUG("dbus_message_is_signal: " << dbus_signal);

		// send signal to display scene
                DisplayScene::pushEvent(nullptr, ActionSignalName, (void*) & dbus_signal);
	    }
        }

	if(st->is_thread)
	  Tools::delay(st->delay);
    }

    return PluginResult::DefaultOk;
}

bool signal_dbus_signal_get_value(void* ptr, int type, void* val)
{
    switch(type)
    {
        case PluginValue::PluginName:
            if(auto res = static_cast<std::string*>(val))
            {
                res->assign("signal_dbus_signal");
                return true;
            }
            break;
    
        case PluginValue::PluginVersion:
            if(auto res = static_cast<int*>(val))
            {
                *res = signal_dbus_signal_version;
                return true;
            }
            break;

        case PluginValue::PluginAPI:
            if(auto res = static_cast<int*>(val))
            {
                *res = PLUGIN_API;
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
        signal_dbus_signal_t* st = static_cast<signal_dbus_signal_t*>(ptr);
        if(4 < st->debug)
            DEBUG("version: " << signal_dbus_signal_version << ", type: " << type);

        switch(type)
        {
            default: break;
        }
    }

    return false;
}

bool signal_dbus_signal_set_value(void* ptr, int type, const void* val)
{
    signal_dbus_signal_t* st = static_cast<signal_dbus_signal_t*>(ptr);
    if(4 < st->debug)
        DEBUG("version: " << signal_dbus_signal_version << ", type: " << type);

    switch(type)
    {
        case PluginValue::SignalStopThread:
            if(st->is_thread)
                st->is_thread = false;
            break;

        default: break;
    }

    return false;
}

#ifdef __cplusplus
}
#endif
