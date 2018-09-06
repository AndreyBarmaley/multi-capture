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

#include <iomanip>
#include <sstream>
#include <cstring>

#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/poll.h>

#include "libraw1394/raw1394.h"
#include "libavc1394/avc1394.h"
#include "libavc1394/rom1394.h"
#include "libiec61883/iec61883.h"

#include "libdv/dv.h"
#include "libdv/dv_types.h"

#include "mpeg2dec/mpeg2.h"
#include "mpeg2dec/mpeg2convert.h"


struct capture_fireware_t
{
    bool 	is_used;
    bool 	is_debug;
    Surface 	surface;

    raw1394handle_t	raw1394;
    struct pollfd	raw1394_poll;

    iec61883_dv_fb_t 	iec61883_dv;
    iec61883_mpeg2_t 	iec61883_mpeg2;
    dv_decoder_t*	dv_decoder;
    mpeg2dec_t*		mpeg_decoder;

    uint64_t		guid;
    int			node;
    int			port;
    int			type;
    int			input;
    int			output;
    int			channel;
    int			bandwidth;
    int 		max_packets;
    int			receiving;
    unsigned char*	rgb_data;
    int			rgb_width;
    int			rgb_height;

    capture_fireware_t() : is_used(false), is_debug(false), raw1394(NULL), iec61883_dv(NULL), iec61883_mpeg2(NULL), dv_decoder(NULL), mpeg_decoder(NULL),
	guid(0), node(-1), port(-1), type(0), input(-1), output(-1), channel(-1), bandwidth(0), max_packets(100), receiving(0), rgb_data(NULL), rgb_width(0), rgb_height(0) {}

    void clear(void)
    {
	is_used = false;
	is_debug = false;
	raw1394 = NULL;
	iec61883_dv = NULL;
	iec61883_mpeg2 = NULL;
	dv_decoder = NULL;
	mpeg_decoder = NULL;
	guid = 0;
	node = -1;
	port = -1;
	type = 0;
	input = -1;
	output = -1;
	channel = -1;
	bandwidth = 0;
	max_packets = 100;
	receiving = 0;
	rgb_data = NULL;
	rgb_width = 0;
	rgb_height = 0;
    	surface.reset();
    }
};

#ifndef CAPTURE_FIREWARE_SPOOL
#define CAPTURE_FIREWARE_SPOOL 4
#endif

#define MOTDCT_SPEC_ID      0x00005068
#define IEC61883_DV         1
#define IEC61883_HDV        2

capture_fireware_t capture_fireware_vals[CAPTURE_FIREWARE_SPOOL];

const char* capture_fireware_get_name(void)
{
    return "capture_fireware";
}

int capture_fireware_get_version(void)
{
    return 20180817;
}

bool capture_fireware_select(void* ptr)
{
    capture_fireware_t* st = static_cast<capture_fireware_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_fireware_get_version());

    raw1394handle_t raw1394 = raw1394_new_handle();

    if(! raw1394)
    {
        ERROR("Failed to open IEEE1394 interface.");
        return false;
    }

    struct raw1394_portinfo pinf[16];
    rom1394_directory rom_dir;

    int nb_ports = raw1394_get_port_info(raw1394, pinf, 16);
    if(0 > nb_ports)
    {
        ERROR("Failed to get number of IEEE1394 ports.");
        return NULL;
    }

    for(int pi = 0; pi < nb_ports; ++pi)
    {
        raw1394_destroy_handle(raw1394);
	raw1394 = raw1394_new_handle_on_port(pi);

        if(! raw1394)
	{
            ERROR("Failed setting IEEE1394 port: " << pi);
            return false;
        }

        for(int ni = 0; ni < raw1394_get_nodecount(raw1394); ++ni)
	{
            /* Select device explicitly by GUID */
            if(st->guid > 1)
	    {
                if(st->guid == rom1394_get_guid(raw1394, ni))
		{
                    st->node = ni;
                    st->port = pi;
		    st->raw1394 = raw1394;
		    return true;
                }
            }
	    else
            /* Select first AV/C tape recorder player node */
            if(0 <= rom1394_get_directory(raw1394, ni, & rom_dir))
	    {
                if((ROM1394_NODE_TYPE_AVC == rom1394_get_node_type(& rom_dir) &&
                        avc1394_check_subunit_type(raw1394, ni, AVC1394_SUBUNIT_TYPE_VCR)) ||
                    rom_dir.unit_spec_id == MOTDCT_SPEC_ID)
		{
                    st->node = ni;
                    st->port = pi;
		    st->raw1394 = raw1394;
		    uint64_t guid = rom1394_get_guid(raw1394, ni);

		    VERBOSE("guid: " << String::hex64(guid));
                }

                rom1394_free_directory(& rom_dir);
		if(st->raw1394) return true;
            }
        }
    }

    return false;
}

int capture_fireware_hdv_callback(unsigned char* data, int length, unsigned int complete, void* ptr)
{
    capture_fireware_t* st = static_cast<capture_fireware_t*>(ptr);

    if(data && 0 < length && st->mpeg_decoder)
    {
	if(st->is_debug)
	    DEBUG("version: " << capture_fireware_get_version() << ", packet length: " << length);

	const mpeg2_info_t* info = mpeg2_info(st->mpeg_decoder);
	mpeg2_state_t state = mpeg2_parse(st->mpeg_decoder);

	switch(state)
	{
	    case STATE_BUFFER:
		mpeg2_buffer(st->mpeg_decoder, data, data + length);
        	break;

	    case STATE_SEQUENCE:
	    {
		int res = mpeg2_convert(st->mpeg_decoder, mpeg2convert_rgb24, NULL);
		if(st->is_debug) VERBOSE("mpeg2_convert: " << res);
	    }
	    break;

	    case STATE_SLICE:
	    case STATE_END:
	    case STATE_INVALID_END:
    		if(info->display_fbuf)
		{
		    int rgbsz = info->sequence->width * info->sequence->height * 3;

		    if(! st->rgb_data)
		    {
			st->rgb_width = info->sequence->width;
			st->rgb_height = info->sequence->height;
			st->rgb_data = new unsigned char [rgbsz];
		    }

		    std::memcpy(st->rgb_data, info->display_fbuf->buf[0], rgbsz);
		}
        	break;

    	    default:
        	break;
    	}
    }

    return 0;
}

int capture_fireware_dv_callback(unsigned char* data, int length, int complete, void* ptr)
{
    capture_fireware_t* st = static_cast<capture_fireware_t*>(ptr);

    if(data && 0 < length && st->dv_decoder)
    {
        int res = dv_parse_header(st->dv_decoder, data);
        dv_parse_packs(st->dv_decoder, data);

	if(st->is_debug)
	    VERBOSE("version: " << capture_fireware_get_version() << ", packet length: " << length << ", complete: " << complete <<
		    ", frame size: " << st->dv_decoder->width << "x" << st->dv_decoder->height << ", dv_parse_header: " << res);

	if(data && 0 < length)
	{
	    if(! st->rgb_data)
	    {
		st->rgb_width = st->dv_decoder->width;
		st->rgb_height = st->dv_decoder->height;
		st->rgb_data = new unsigned char [st->rgb_width * st->rgb_height * 3];
	    }

    	    unsigned char* pixels[3];
    	    int pitches[3];

	    pixels[0] = st->rgb_data;
    	    pixels[1] = NULL;
    	    pixels[2] = NULL;

    	    pitches[0] = st->dv_decoder->width * 3;
    	    pitches[1] = 0;
    	    pitches[2] = 0;

    	    dv_decode_full_frame(st->dv_decoder, data, e_dv_color_rgb, pixels, pitches);
	}
    }

    return 0;
}

void* capture_fireware_init(const JsonObject & config)
{
    VERBOSE("version: " << capture_fireware_get_version());

    int devindex = 0;
    for(; devindex < CAPTURE_FIREWARE_SPOOL; ++devindex)
        if(! capture_fireware_vals[devindex].is_used) break;

    if(CAPTURE_FIREWARE_SPOOL <= devindex)
    {
        ERROR("spool is busy, max limit: " << CAPTURE_FIREWARE_SPOOL);
        return NULL;
    }

    DEBUG("spool index: " << devindex);
    capture_fireware_t* st = & capture_fireware_vals[devindex];

    st->is_debug = config.getBoolean("debug", false);
    std::string strguid = config.getString("guid");

    if(strguid.size() && strguid != "auto")
    {
	std::istringstream ss(strguid);
	ss >> std::hex >> st->guid;

	if(ss.fail())
	{
            ERROR("invalid dv guid parameter: " << strguid);
	    st->guid = 0;
        }
	else
	{
	    DEBUG("params: " << "guid = " << strguid);
	}
    }

    if(! capture_fireware_select(st))
    {
        ERROR("No AV/C devices found.");
	return NULL;
    }

    // Provide bus sanity for multiple connections
    iec61883_cmp_normalize_output(st->raw1394, 0xffc0 | st->node);

    // Find out if device is DV or HDV
    int response = avc1394_transaction(st->raw1394, st->node,
                                       AVC1394_CTYPE_STATUS |
                                       AVC1394_SUBUNIT_TYPE_TAPE_RECORDER |
                                       AVC1394_SUBUNIT_ID_0 |
                                       AVC1394_VCR_COMMAND_OUTPUT_SIGNAL_MODE | 0xFF, 2);
    response = AVC1394_GET_OPERAND0(response);
    st->type = (response == 0x10 || response == 0x90 || response == 0x1A || response == 0x9A) ?
		IEC61883_HDV : IEC61883_DV;

    // Connect to device, and do initialization
    st->channel = iec61883_cmp_connect(st->raw1394, st->node, & st->output,
                                       raw1394_get_local_id(st->raw1394), & st->input, & st->bandwidth);
    if(st->channel < 0)
        st->channel = 63;

    if(st->type == IEC61883_HDV)
    {
        st->iec61883_mpeg2 = iec61883_mpeg2_recv_init(st->raw1394, capture_fireware_hdv_callback, st);
	st->max_packets *= 766;
    }
    else
    {
        st->iec61883_dv = iec61883_dv_fb_init(st->raw1394, capture_fireware_dv_callback, st);
    }

    st->raw1394_poll.fd = raw1394_get_fd(st->raw1394);
    st->raw1394_poll.events = POLLIN | POLLERR | POLLHUP | POLLPRI;

    // actually start receiving
    if (st->type == IEC61883_HDV)
    {
	st->mpeg_decoder = mpeg2_init();

	if(! st->mpeg_decoder)
	    ERROR("mpeg decoder error");

        iec61883_mpeg2_recv_start(st->iec61883_mpeg2, st->channel);
	VERBOSE("mpeg2 mode started");
    }
    else
    {
        st->dv_decoder = dv_decoder_new(0, 0, 0);
        st->dv_decoder->quality = DV_QUALITY_BEST;

	if(! st->dv_decoder)
	    ERROR("DV decoder error");

        iec61883_dv_fb_start(st->iec61883_dv, st->channel);
	VERBOSE("DV mode started");
    }

    st->is_used = true;

    return st;
}

void capture_fireware_quit(void* ptr)
{
    capture_fireware_t* st = static_cast<capture_fireware_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_fireware_get_version());

    if(st->raw1394)
    {
	if(st->type == IEC61883_HDV)
	{
	    if(st->mpeg_decoder) mpeg2_close(st->mpeg_decoder);

    	    iec61883_mpeg2_recv_stop(st->iec61883_mpeg2);
    	    iec61883_mpeg2_close(st->iec61883_mpeg2);
	}
	else
	{
	    if(st->dv_decoder) dv_decoder_free(st->dv_decoder);

    	    iec61883_dv_fb_stop(st->iec61883_dv);
    	    iec61883_dv_fb_close(st->iec61883_dv);
	}

	iec61883_cmp_disconnect(st->raw1394, st->node, st->output,
                            raw1394_get_local_id(st->raw1394), st->input, st->channel, st->bandwidth);

	raw1394_destroy_handle(st->raw1394);
    }

    if(st->rgb_data)
	delete [] st->rgb_data;

    st->clear();
}

int capture_fireware_frame_action(void* ptr)
{
    capture_fireware_t* st = static_cast<capture_fireware_t*>(ptr);
    if(st->is_debug) DEBUG("version: " << capture_fireware_get_version());
    int result = 0;

    while((result = poll(& st->raw1394_poll, 1, 200)) < 0)
    {
        if(! (errno == EAGAIN || errno == EINTR))
	{
    	    ERROR("raw1394 poll error occurred");
    	    return -1;
        }
    }

    if(result > 0 && ((st->raw1394_poll.revents & POLLIN) || (st->raw1394_poll.revents & POLLPRI)))
    {
	st->receiving = 1;
        raw1394_loop_iterate(st->raw1394);

	if(st->rgb_data)
	{
    	    SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(st->rgb_data, st->rgb_width, st->rgb_height, 24, st->rgb_width * 3, SDL_PIXELFORMAT_RGB24);
            st->surface = Surface(sf).copy();

	    delete [] st->rgb_data;
	    st->rgb_data = NULL;
	}

	return 0;
    }
    else
    if(st->receiving)
    {
        ERROR("no more input data available");
	return -1;
    }

    return -1;
}

const Surface & capture_fireware_get_surface(void* ptr)
{
    capture_fireware_t* st = static_cast<capture_fireware_t*>(ptr);

    if(st->is_debug) DEBUG("version: " << capture_fireware_get_version());
    return st->surface;
}

#ifdef __cplusplus
}
#endif
