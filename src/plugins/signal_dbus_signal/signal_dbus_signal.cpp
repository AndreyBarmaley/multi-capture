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

#include <cstring>

#include "../../settings.h"
#include "dbus/dbus.h"

#ifdef __cplusplus
extern "C" {
#endif

struct signal_dbus_signal_t
{
    bool	is_used;
    bool        is_debug;
    std::string system_signal;
    std::string dbus_name;
    std::string dbus_object;
    std::string dbus_interface;
    std::string dbus_signal;
    std::string dbus_autostart;
    DBusConnection* dbus_conn;

    signal_dbus_signal_t() : is_used(false), is_debug(false), dbus_conn(NULL) {}

    void clear(void)
    {
        is_used = false;
        is_debug = false;
        system_signal.clear();
	dbus_name.clear();
	dbus_interface.clear();
	dbus_signal.clear();
	dbus_conn = NULL;
    }
};

#ifndef DBUS_SIGNAL_SPOOL
#define DBUS_SIGNAL_SPOOL 16
#endif

signal_dbus_signal_t signal_dbus_signal_vals[DBUS_SIGNAL_SPOOL];

const char* signal_dbus_signal_get_name(void)
{
    return "signal_dbus_signal";
}

int signal_dbus_signal_get_version(void)
{
    return 20210130;
}

int signal_dbus_autostart(signal_dbus_signal_t* st)
{
    if(st->is_debug) DEBUG("version: " << signal_dbus_signal_get_version());

    // create a new method call and check for errors
    DBusMessage* msg = dbus_message_new_method_call(st->dbus_name.c_str(), st->dbus_object.c_str(), st->dbus_interface.c_str(), st->dbus_autostart.c_str());

    if(NULL == msg)
    {
	ERROR("message is null");
	return 0;
    }

    // send message and get a handle for a reply
    DBusPendingCall* pending;
    if(!dbus_connection_send_with_reply (st->dbus_conn, msg, &pending, -1  /* -1 is default timeout */))
    {
	ERROR("out of memory");
	return 0;
    }

    if(NULL == pending)
    {
	ERROR("pending call is null");
	return 0;
    }
    dbus_connection_flush(st->dbus_conn);
    if(st->is_debug) VERBOSE("request sent");

    // free message
    dbus_message_unref(msg);

    // block until we recieve a reply
    dbus_pending_call_block(pending);

    // get the reply message
    msg = dbus_pending_call_steal_reply(pending);
    if(NULL == msg)
    {
	ERROR("reply is null");
	return 0;
    }

    // free the pending message handle
    dbus_pending_call_unref(pending);
    dbus_message_unref(msg);

   return 1;
}

void* signal_dbus_signal_init(const JsonObject & config)
{
    VERBOSE("version: " << signal_dbus_signal_get_version());

    int devindex = 0;
    for(; devindex < DBUS_SIGNAL_SPOOL; ++devindex)
        if(! signal_dbus_signal_vals[devindex].is_used) break;

    if(DBUS_SIGNAL_SPOOL <= devindex)
    {
        ERROR("spool is busy, max limit: " << DBUS_SIGNAL_SPOOL);
        return NULL;
    }

    DEBUG("spool index: " << devindex);
    signal_dbus_signal_t* st = & signal_dbus_signal_vals[devindex];

    st->is_used = true;
    st->is_debug = config.getBoolean("debug", false);
    st->system_signal = config.getString("signal");
    bool dbusIsSystem = config.getBoolean("dbus:system", false);
 
    // dbus init
    DBusError dbus_err;
    dbus_error_init(&dbus_err);

    // connect to the bus and check for errors
    st->dbus_conn = dbus_bus_get((dbusIsSystem ? DBUS_BUS_SYSTEM : DBUS_BUS_SESSION), &dbus_err);
    if(dbus_error_is_set(&dbus_err))
    {
	st->is_used = false;
	ERROR("dbus_bus_get: " << dbus_err.message);
	dbus_error_free(&dbus_err);
        return NULL;
    }

    if(NULL == st->dbus_conn)
    {
	st->is_used = false;
	ERROR("dbus_bus_get: " << "is null");
        return NULL;
    }

    st->dbus_name = config.getString("dbus:name", "org.test.dbus");
    st->dbus_object = config.getString("dbus:object", "/org/test/dbus/object");
    st->dbus_interface = config.getString("dbus:interface", "org.test.dbus.interface");
    st->dbus_signal = config.getString("dbus:signal", "test_signal");
    st->dbus_autostart = config.getString("dbus:autostart");

    int ret = dbus_bus_request_name(st->dbus_conn, st->dbus_name.c_str(), DBUS_NAME_FLAG_REPLACE_EXISTING , &dbus_err);
    if(dbus_error_is_set(&dbus_err))
    {
	st->is_used = false;
	ERROR("dbus_bus_request_name: " << dbus_err.message);
	dbus_error_free(&dbus_err);
        return NULL;
    }

    if(0 > ret)
    {
	st->is_used = false;
	ERROR("dbus_bus_request_name: " << ret);
        return NULL;
    }

    if(st->is_debug)
    {
	VERBOSE("dbus_bus_request_name: " << ret);
    }

    // add a rule for which messages we want to see
    std::string matchForm = StringFormat("type='%1',interface='%2'").arg("signal").arg(st->dbus_interface);
    dbus_bus_add_match(st->dbus_conn, matchForm.c_str(), &dbus_err);
    dbus_connection_flush(st->dbus_conn);
    if(dbus_error_is_set(&dbus_err))
    {
	st->is_used = false;
	ERROR("match error: " << dbus_err.message);
	dbus_error_free(&dbus_err);
        return NULL;
    }

    DEBUG("params: " << "signal = " << st->system_signal);
    DEBUG("params: " << "dbus:system" << " = " << (dbusIsSystem ? "true" : "false"));
    DEBUG("params: " << "dbus:name" << " = " << st->dbus_name);
    DEBUG("params: " << "dbus:object" << " = " << st->dbus_object);
    DEBUG("params: " << "dbus:interface" << " = " << st->dbus_interface);
    DEBUG("params: " << "dbus:match" << " = " << matchForm);
    DEBUG("params: " << "dbus:signal" << " = " << st->dbus_signal);

    if(dbus_error_is_set(&dbus_err))
	dbus_error_free(&dbus_err);

    if(st->dbus_autostart.size())
	signal_dbus_autostart(st);

    DEBUG("init complete");

    //st->is_used = true;
    return st;
}

void signal_dbus_signal_quit(void* ptr)
{
    signal_dbus_signal_t* st = static_cast<signal_dbus_signal_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_dbus_signal_get_version());
    st->clear();
}

int signal_dbus_signal_action(void* ptr)
{
    signal_dbus_signal_t* st = static_cast<signal_dbus_signal_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << signal_dbus_signal_get_version());

    int loop = 1;

    // loop listening for signals being emmitted
    while(loop)
    {
	// non blocking read of the next available message
	dbus_connection_read_write(st->dbus_conn, 0);
    	DBusMessage* dbus_msg = dbus_connection_pop_message(st->dbus_conn);

	// loop again if we haven't read a message
	if(NULL == dbus_msg)
	{
	    // 10 ms
	    Tools::delay(100);
	    continue;
	}


	bool sendSignal = false;
	// check if the message is a signal from the correct interface and with the correct name
	if(dbus_message_is_signal(dbus_msg, st->dbus_interface.c_str(), st->dbus_signal.c_str()))
	{
	    if(st->is_debug) DEBUG("dbus_message_is_signal: " << "true");
	    sendSignal = true;

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
	else
	if(st->is_debug)
	{
	    DEBUG("dbus_message_is_signal: " << "false");
	}

	// free the message
	dbus_message_unref(dbus_msg);

	if(sendSignal)
	{
            DisplayScene::pushEvent(NULL, ActionBackSignal, & st->dbus_signal);
	}
    }

    return 0;
}

#ifdef __cplusplus
}
#endif
