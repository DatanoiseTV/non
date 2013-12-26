
/*******************************************************************************/
/* Copyright (C) 2008 Jonathan Moore Liles                                     */
/*                                                                             */
/* This program is free software; you can redistribute it and/or modify it     */
/* under the terms of the GNU General Public License as published by the       */
/* Free Software Foundation; either version 2 of the License, or (at your      */
/* option) any later version.                                                  */
/*                                                                             */
/* This program is distributed in the hope that it will be useful, but WITHOUT */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for   */
/* more details.                                                               */
/*                                                                             */
/* You should have received a copy of the GNU General Public License along     */
/* with This program; see the file COPYING.  If not,write to the Free Software */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */
/*******************************************************************************/

#pragma once

#include <atomic>
#include <sigc++/sigc++.h>

using namespace sigc;

#include <MIDI/types.h> // just for tick_t

#include <jack/transport.h>

typedef double playhead_t;

/* Timebase variables from jack_transport_query and calculated derivatives */
typedef struct
{
    bool rolling;
    bool valid;

    unsigned long bar;
    unsigned beat;
    unsigned tick;

    playhead_t ticks;

    unsigned beats_per_bar;
    unsigned beat_type;
    double ticks_per_beat;
    double beats_per_minute;

    double frame_rate;
    double frame;

    double frames_per_tick;
} Timebase;

class Transport : public trackable {
    /* Accessed by UI and RT threads */
    std::atomic_uint _master_beats_per_minute;                  /* In 10ths of BPM so we can use atomic int */
    std::atomic_uint _master_beats_per_bar;
    std::atomic_uint _master_beat_type;

public:

    Timebase rt;                                                /* RT thread timebase variables */
    Timebase ui;                                                /* UI thread timebase variables */

    std::atomic_bool master;                                    /* are we driving the transport?  Accessed by UI and RT threads */
    std::atomic_bool recording;                                 /* Are we recording?  Accessed by UI and RT threads */

    Transport ( void );

    void poll_rt ( void );
    void poll_ui ( void );
    void start ( void );
    void stop ( void );
    void toggle ( void );
    void locate ( tick_t ticks );

    void set_beats_per_minute ( double n );
    void set_beats_per_bar ( int n );
    void set_beat_type ( int n );

    static void timebase ( jack_transport_state_t state, jack_nframes_t nframes, jack_position_t *pos, int new_pos, void *arg );
};

extern Transport transport;
