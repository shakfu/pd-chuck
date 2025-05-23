/*----------------------------------------------------------------------------
  ChucK Strongly-timed Audio Programming Language
    Compiler, Virtual Machine, and Synthesis Engine

  Copyright (c) 2003 Ge Wang and Perry R. Cook. All rights reserved.
    http://chuck.stanford.edu/
    http://chuck.cs.princeton.edu/

  This program is free software; you can redistribute it and/or modify
  it under the dual-license terms of EITHER the MIT License OR the GNU
  General Public License (the latter as published by the Free Software
  Foundation; either version 2 of the License or, at your option, any
  later version).

  This program is distributed in the hope that it will be useful and/or
  interesting, but WITHOUT ANY WARRANTY; without even the implied warranty
  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  MIT Licence and/or the GNU General Public License for details.

  You should have received a copy of the MIT License and the GNU General
  Public License (GPL) along with this program; a copy of the GPL can also
  be obtained by writing to the Free Software Foundation, Inc., 59 Temple
  Place, Suite 330, Boston, MA 02111-1307 U.S.A.
-----------------------------------------------------------------------------*/

//-----------------------------------------------------------------------------
// name: chuck_audio.h
// desc: chuck host digital audio I/O, using RtAudio (from Gary Scavone)
//
// author: Ge Wang (ge@ccrma.stanford.edu | gewang@cs.princeton.edu)
//         Spencer Salazar (spencer@ccrma.stanford.edu)
// RtAudio by: Gary Scavone
// date: Spring 2004
//-----------------------------------------------------------------------------
#ifndef __CHUCK_AUDIO_H__
#define __CHUCK_AUDIO_H__

#include "chuck_def.h"
#include <string>
#include "RtAudio/RtAudio.h"




//-----------------------------------------------------------------------------
// define, defaults, forward references
//-----------------------------------------------------------------------------
// sample rate defaults by platform
#if defined(__PLATFORM_LINUX__)         // linux
  #define CK_SAMPLE_RATE_DEFAULT        48000
  #define CK_BUFFER_SIZE_DEFAULT        256
#elif defined(__PLATFORM_APPLE__)      // macOS
  #define CK_SAMPLE_RATE_DEFAULT        44100
  #define CK_BUFFER_SIZE_DEFAULT        256
#else                                   // windows & everywhere else
  #define CK_SAMPLE_RATE_DEFAULT        44100
  #define CK_BUFFER_SIZE_DEFAULT        512
#endif

// number of channels
#define CK_NUM_CHANNELS_DEFAULT         2
// number buffers
#define CK_NUM_BUFFERS_DEFAULT          8


//-----------------------------------------------------------------------------
// audio callback definition
//-----------------------------------------------------------------------------
typedef void (* ck_f_audio_cb)( SAMPLE * input, SAMPLE * output,
    t_CKUINT numFrames, t_CKUINT numInChans, t_CKUINT numOutChans,
    void * userData );


// real-time watch dog
extern t_CKBOOL g_do_watchdog;
// countermeasure
extern t_CKUINT g_watchdog_countermeasure_priority;
// watchdog timeout
extern t_CKFLOAT g_watchdog_timeout;




//-----------------------------------------------------------------------------
// name: class ChuckAudioDriverInfo
// desc: info pertaining to an audio driver
//-----------------------------------------------------------------------------
struct ChuckAudioDriverInfo
{
    // driver API ID; e.g., RtAudio::Api values
    // RtAudio::MACOSX_CORE or RtAudio::WINDOWS_DS or RtAudio::UNIX_JACK
    // (see RtAudio.h for full list)
    t_CKUINT driver;
    // a short name of the driver, e.g., "DS"
    std::string name;
    // a longer name of the driver, e.g., "DirectSound"
    std::string userFriendlyName;

    // constructor
    ChuckAudioDriverInfo()
        : driver(0),
        name("(unspecified)"),
        userFriendlyName("(unspecified driver")
    {  }
};




//-----------------------------------------------------------------------------
// name: class ChuckAudio
// desc: chuck host audio I/O
//-----------------------------------------------------------------------------
class ChuckAudio
{
public:
    static t_CKBOOL initialize( t_CKUINT dac_device,
                                t_CKUINT adc_device,
                                t_CKUINT num_dac_channels,
                                t_CKUINT num_adc_channels,
                                t_CKUINT sample_rate,
                                t_CKUINT buffer_size,
                                t_CKUINT num_buffers,
                                ck_f_audio_cb callback,
                                void * data,
                                t_CKBOOL force_srate, // force_srate | 1.3.1.2 (added)
                                const char * driver // NULL means default for build | 1.5.0.0 (added)
                                );
    static void shutdown( t_CKUINT msWait = 0 );
    static t_CKBOOL start();
    static t_CKBOOL stop();

public: // watchdog stuff
    static t_CKBOOL watchdog_start();
    static t_CKBOOL watchdog_stop();

public: // driver related operations
    // default audio driver number
    static RtAudio::Api defaultDriverApi();
    // default audio driver name
    static const char * defaultDriverName();
    // get API/driver enum from name
    static RtAudio::Api driverNameToApi( const char * driver = NULL );
    // get API/drive name from int assumed to be RtAudio::Api enum
    static const char * driverApiToName( t_CKUINT driverNum );
    // get number of compiled audio driver APIs
    static t_CKUINT numDrivers();
    // get info on a particular driver
    static ChuckAudioDriverInfo getDriverInfo( t_CKUINT n );

public: // audio device related operations
    // probe audio devices; NULL for driver means default for build
    static void probe( const char * driver );
    // get device number by name?
    // 1.4.2.0: changed return type from t_CKUINT to t_CKINT
    static t_CKINT device_named( const char * driver,
                                 const std::string & name,
                                 t_CKBOOL needs_dac = FALSE,
                                 t_CKBOOL needs_adc = FALSE );

public:
    static t_CKUINT srate() { return m_sample_rate; }
    static t_CKUINT num_channels_out() { return m_num_channels_out; }
    static t_CKUINT num_channels_in() { return m_num_channels_in; }
    static t_CKUINT dac_num() { return m_dac_n; }
    static t_CKUINT adc_num() { return m_adc_n; }
    static t_CKUINT bps() { return m_bps; }
    static t_CKUINT buffer_size() { return m_buffer_size; }
    static t_CKUINT num_buffers() { return m_num_buffers; }
    static RtAudio * audio() { return m_rtaudio; }

    static void set_extern( SAMPLE * in, SAMPLE * out )
        { m_extern_in = in; m_extern_out = out; }
    static int cb( void * output_buffer, void * input_buffer, unsigned int buffer_size,
        double streamTime, RtAudioStreamStatus status, void * user_data );

public: // data
    static t_CKBOOL m_init;
    static t_CKBOOL m_start;
    static t_CKBOOL m_go; // counter of # of times callback called
    static t_CKBOOL m_silent; // if true, will output silence
    static t_CKBOOL m_expand_in_mono2stereo; // 1.4.0.1 for in:1 out:2
    static t_CKUINT m_num_channels_out;
    static t_CKUINT m_num_channels_in;
    static t_CKUINT m_num_channels_max; // the max of out/in
    static t_CKUINT m_sample_rate;
    static t_CKUINT m_bps;
    static t_CKUINT m_buffer_size;
    static t_CKUINT m_num_buffers;
    static SAMPLE * m_buffer_out;
    static SAMPLE * m_buffer_in;
    static SAMPLE * m_extern_in; // for things like audicle
    static SAMPLE * m_extern_out; // for things like audicle

    static RtAudio * m_rtaudio;
    static t_CKINT m_dac_n; // 1.5.0.3 (ge) changed to signed
    static t_CKINT m_adc_n; // 1.5.0.3 (ge) changed to signed
    static std::string m_dac_name;
    static std::string m_adc_name;
    static std::string m_driver_name;
    static ck_f_audio_cb m_audio_cb;
    static void * m_cb_user_data;
};




#endif
