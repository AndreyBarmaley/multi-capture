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
#include <cstring>

#include "../../settings.h"
#include "dbus/dbus.h"

#ifdef __cplusplus
extern "C" {
#endif

struct signal_dbus_signal_t
{
    bool        is_thread;
    bool        is_debug;
    int         delay;
    std::string signal;
    std::string dbus_name;
    std::string dbus_object;
    std::string dbus_interface;
    StringList  dbus_signals;
    std::string dbus_autostart;
    DBusConnection* dbus_conn;

    signal_dbus_signal_t() : is_thread(true), is_debug(false), delay(100), dbus_conn(nullptr) {}
    ~signal_dbus_signal_t()
    {
	clear();
    }

    bool init(bool isSystem)
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

	int ret = dbus_bus_request_name(dbus_conn, dbus_name.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING , &dbus_err);
	if(dbus_error_is_set(&dbus_err))
	{
	    ERROR("dbus_bus_request_name: " << dbus_err.message);
	    dbus_error_free(&dbus_err);
    	    return false;
	}

	if(is_debug)
	{
	    VERBOSE("dbus_bus_request_name: ret code: " << ret);
	}

	if(0 > ret)
    	    return false;

	// add a rule for which messages we want to see
	std::string matchForm = StringFormat("type='%1',interface='%2'").arg("signal").arg(dbus_interface);
	DEBUG("math form: " << matchForm);

	dbus_bus_add_match(dbus_conn, matchForm.c_str(), &dbus_err);
	dbus_connection_flush(dbus_conn);
	if(dbus_error_is_set(&dbus_err))
	{
	    ERROR("match error: " << dbus_err.message);
	    dbus_error_free(&dbus_err);
    	    return false;
	}

	if(dbus_error_is_set(&dbus_err))
	    dbus_error_free(&dbus_err);

	return true;
    }

    int dbusSignalAutostart(void)
    {
	// create a new method call and check for errors
	DBusMessage* msg = dbus_message_new_method_call(dbus_name.c_str(), dbus_object.c_str(), dbus_interface.c_str(), dbus_autostart.c_str());

	if(! msg)
	{
	    ERROR("message is null");
	    return 0;
	}

	// send message and get a handle for a reply
	DBusPendingCall* pending;
	if(!dbus_connection_send_with_reply (dbus_conn, msg, &pending, -1  /* -1 is default timeout */))
	{
	    ERROR("out of memory");
	    return 0;
	}

	if(! pending)
	{
	    ERROR("pending call is null");
	    return 0;
	}

	dbus_connection_flush(dbus_conn);
	if(is_debug) VERBOSE("request sent");

	// free message
	dbus_message_unref(msg);

	// block until we recieve a reply
	dbus_pending_call_block(pending);

	// get the reply message
	msg = dbus_pending_call_steal_reply(pending);
	if(! msg)
	{
	    ERROR("reply is null");
	    return 0;
	}

	// free the pending message handle
	dbus_pending_call_unref(pending);
	dbus_message_unref(msg);

      return 1;
    }

    void clear(void)
    {
        is_thread = true;
        is_debug = false;
        delay = 100;
	signal.clear();
	dbus_name.clear();
	dbus_interface.clear();
	dbus_signals.clear();
	dbus_conn = nullptr;
    }
};

const char* signal_dbus_signal_get_name(void)
{
    return "signal_dbus_signal";
}

int signal_dbus_signal_get_version(void)
{
    return 20220205;
}

void* signal_dbus_signal_init(const JsonObject & config)
{
    VERBOSE("version: " << signal_dbus_signal_get_version());

    auto ptr = std::make_unique<signal_dbus_signal_t>();

    ptr->is_debug = config.getBoolean("debug", false);
    ptr->delay = config.getInteger("delay", 100);
    bool dbusIsSystem = config.getBoolean("dbus:system", false);
 
    ptr->dbus_name = config.getString("dbus:name", "org.test.dbus");
    ptr->dbus_object = config.getString("dbus:object", "/org/test/dbus/object");
    ptr->dbus_interface = config.getString("dbus:interface", "org.test.dbus.interface");
    ptr->dbus_autostart = config.getString("dbus:autostart");
    ptr->dbus_signals = config.getStdList<std::string>("dbus:signals");
    ptr->dbus_signals.emplace_back(config.getString("dbus:signal", "test_signal"));

    if(! ptr->init(dbusIsSystem))
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
    if(st->is_debug) DEBUG("version: " << signal_dbus_signal_get_version());

    delete st;
}

void signal_dbus_signal_stop_thread(void* ptr)
{
    signal_dbus_signal_t* st = static_cast<signal_dbus_signal_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_dbus_signal_get_version());

    if(st->is_thread)
	st->is_thread = false;
}

int signal_dbus_signal_action(void* ptr)
{
    signal_dbus_signal_t* st = static_cast<signal_dbus_signal_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_dbus_signal_get_version());

    // loop listening for signals being emmitted
    while(st->is_thread)
    {
	// non blocking read of the next available message
	dbus_connection_read_write(st->dbus_conn, 0);
    	DBusMessage* dbus_msg = dbus_connection_pop_message(st->dbus_conn);

	// loop again if we haven't read a message
	if(! dbus_msg)
	{
	    Tools::delay(st->delay);
	    continue;
	}

        for(auto & dbus_signal : st->dbus_signals)
	{
            // check if the message is a signal from the correct interface and with the correct name
	    if(dbus_message_is_signal(dbus_msg, st->dbus_interface.c_str(), dbus_signal.c_str()))
	    {
	        if(st->is_debug) DEBUG("dbus_message_is_signal: " << dbus_signal);

                st->signal = dbus_signal;

		// send signal to display scene
                DisplayScene::pushEvent(nullptr, ActionSignalBack, st);

	        /*
	            DBusMessageIter dbus_args;

	            // read the parameters
	            if(! dbus_message_iter_init(msg, &args))
        	        fprintf(stderr, "Message Has No Parameters\n");
	            else
	            if(DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args))
		        fprintf(stderr, "Argument is not string!\n");
	            else
        	        dbus_message_iter_get_basic(&args, &sigvalue);
	        */
	    }
        }

	// free the message
	dbus_message_unref(dbus_msg);

	if(st->is_thread)
	  Tools::delay(st->delay);
    }

    return 0;
}

const std::string & signal_dbus_signal_get_signal(void* ptr)
{
    signal_dbus_signal_t* st = static_cast<signal_dbus_signal_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_dbus_signal_get_version());

    return st->signal;
}

#ifdef __cplusplus
}
#endif
