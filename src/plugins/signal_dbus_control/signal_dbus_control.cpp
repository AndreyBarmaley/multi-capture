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
#include <memory>
#include <cstring>

#include "../../settings.h"
#include "dbus/dbus.h"

#ifdef __cplusplus
extern "C" {
#endif

const int signal_dbus_control_version = 20220412;

#ifdef SWE_SDL12
union DBusBasicValue
{
  unsigned char bytes[8];
  dbus_int16_t  i16;
  dbus_uint16_t u16;
  dbus_int32_t  i32;
  dbus_uint32_t u32;
  dbus_bool_t   bool_val;
  dbus_int64_t  i64;
  dbus_uint64_t u64;
  //DBus8ByteStruct eight;
  double dbl;
  unsigned char byt;
  char *str;
  int fd;
};
#endif

struct signal_dbus_control_t
{
    bool        is_thread;
    int         debug;
    int         delay;
    const char* dbus_name;
    const char* dbus_object;
    const char* dbus_interface;
    std::string signal;
    SessionIdName sessionResetParams;
    DBusConnection* dbus_conn;

    signal_dbus_control_t() : is_thread(true), debug(0), delay(100),
        dbus_name("com.github.AndreyBarmaley.MultiCapture"),
        dbus_object("/com/github/AndreyBarmaley/MultiCapture"),
        dbus_interface("com.github.AndreyBarmaley.MultiCapture"), dbus_conn(nullptr) {}

    ~signal_dbus_control_t()
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

	int ret = dbus_bus_request_name(dbus_conn, dbus_name, DBUS_NAME_FLAG_REPLACE_EXISTING , &dbus_err);
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

	// add a rule for which messages we want to see
        for(auto & sig : { "signal", "method_call" })
        {
	    std::string matchForm = StringFormat("type='%1',interface='%2'").arg(sig).arg(dbus_interface);
	    DEBUG("add math form: " << matchForm);

	    dbus_bus_add_match(dbus_conn, matchForm.c_str(), &dbus_err);
	    if(dbus_error_is_set(&dbus_err))
	    {
	        ERROR("dbus_bus_add_match: " << dbus_err.message);
	        dbus_error_free(&dbus_err);
    	        return false;
	    }
        }

	dbus_connection_flush(dbus_conn);

	return true;
    }

    void clear(void)
    {
        is_thread = true;
        debug = 0;
        delay = 100;
	dbus_conn = nullptr;
    }

    bool read_params_session_reset(DBusMessage* msg)
    {
        DBusMessageIter args;
        DBusBasicValue value;

        // check not empty
        if(! dbus_message_iter_init(msg, &args))
            return false;

        // require: session id uint32
        if(DBUS_TYPE_UINT32 != dbus_message_iter_get_arg_type(&args))
            return false;

        dbus_message_iter_get_basic(&args, & value);
        sessionResetParams.id = value.u32;

        if(! dbus_message_iter_next(&args))
            return false;

        // require: session name string
        if(DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&args))
            return false;

        dbus_message_iter_get_basic(&args, & value);
        sessionResetParams.name.assign(value.str);

        return true;
    }
};

void* signal_dbus_control_init(const JsonObject & config)
{
    VERBOSE("version: " << signal_dbus_control_version);

    auto ptr = std::make_unique<signal_dbus_control_t>();

    ptr->debug = config.getInteger("debug", 0);
    ptr->delay = config.getInteger("delay", 100);

    bool dbusIsSystem = config.getBoolean("dbus:system", false);

    if(! ptr->init(dbusIsSystem))
	return nullptr;

    DEBUG("params: " << "delay = " << ptr->delay);
    DEBUG("params: " << "dbus:system" << " = " << String::Bool(dbusIsSystem));
    DEBUG("params: " << "dbus:name" << " = " << ptr->dbus_name);
    DEBUG("params: " << "dbus:object" << " = " << ptr->dbus_object);
    DEBUG("params: " << "dbus:interface" << " = " << ptr->dbus_interface);

    return ptr.release();
}

void signal_dbus_control_quit(void* ptr)
{
    signal_dbus_control_t* st = static_cast<signal_dbus_control_t*>(ptr);
    if(st->debug) DEBUG("version: " << signal_dbus_control_version);

    delete st;
}

int signal_dbus_control_action(void* ptr)
{
    signal_dbus_control_t* st = static_cast<signal_dbus_control_t*>(ptr);
    if(3 < st->debug) DEBUG("version: " << signal_dbus_control_version);

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

        // check if the message is a signal from the correct interface and with the correct name
	if(dbus_message_is_signal(msg.get(), st->dbus_interface, "programExit"))
	{
	    if(2 < st->debug) DEBUG("dbus_message_is_signal: " << "programExit");

	    // send signal to display scene
            DisplayScene::pushEvent(nullptr, ActionProgramExit, st);
        }
        else
	if(dbus_message_is_method_call(msg.get(), st->dbus_interface, "sessionReset"))
	{
            if(st->read_params_session_reset(msg.get()))
            {
                DisplayScene::pushEvent(nullptr, ActionSessionReset, & st->sessionResetParams);
            }
            else
            {
                ERROR("sessionReset: invalid arguments");
            }
         }

	if(st->is_thread)
	  Tools::delay(st->delay);
    }

    return PluginResult::DefaultOk;
}

bool signal_dbus_control_get_value(void* ptr, int type, void* val)
{
    switch(type)
    {
        case PluginValue::PluginName:
            if(auto res = static_cast<std::string*>(val))
            {
                res->assign("signal_dbus_control");
                return true;
            }
            break;

        case PluginValue::PluginVersion:
            if(auto res = static_cast<int*>(val))
            {
                *res = signal_dbus_control_version;
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
        signal_dbus_control_t* st = static_cast<signal_dbus_control_t*>(ptr);
        if(4 < st->debug)
            DEBUG("version: " << signal_dbus_control_version << ", type: " << type);

        switch(type)
        {
            default: break;
        }
    }

    return false;
}

bool signal_dbus_control_set_value(void* ptr, int type, const void* val)
{
    signal_dbus_control_t* st = static_cast<signal_dbus_control_t*>(ptr);
    if(4 < st->debug)
        DEBUG("version: " << signal_dbus_control_version << ", type: " << type);

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
