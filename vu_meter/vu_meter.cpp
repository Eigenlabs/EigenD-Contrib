/*
 Copyright 2012 Eigenlabs Ltd.  http://www.eigenlabs.com

 This file is part of EigenD.

 EigenD is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 EigenD is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with EigenD.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <piw/piw_cfilter.h>
#include <piw/piw_data.h>
#include <piw/piw_status.h>
#include <pibelcanto/state.h>
#include <picross/pic_flipflop.h>
#include <picross/pic_float.h>
#include <cmath>

#include "vu_meter.h"

#define IN_AUDIO 11
#define IN_MASK SIG1(IN_AUDIO)

#define OUT_LIGHT 56
#define OUT_MASK SIG1(OUT_LIGHT)

#define BLANK_REGION BCTSTATUS_OFF
#define SIGNAL_REGION BCTSTATUS_ACTIVE
#define HIGH_REGION BCTSTATUS_UNKNOWN
#define CLIP_REGION BCTSTATUS_SELECTOR_OFF

#define NOT_ON_SEGMENT 100.0 //just a big value

namespace
{

	struct segment_data_t
	{
		float signal_level;
		float high_level;
		float clip_level;
		int index;
	};

	typedef pic::lcklist_t<segment_data_t>::lcktype segment_list_t;


    struct vu_meter_func_t: piw::cfilterfunc_t
    {
    	const vu_meter::vu_meter_t::impl_t* impl_;

        vu_meter_func_t(const piw::data_t &path, const vu_meter::vu_meter_t::impl_t* impl)
        {
        	impl_ = impl;
            clip_hold_stop_time = 0;
        }

        bool cfilterfunc_start(piw::cfilterenv_t *env, const piw::data_nb_t &id)
        {
            id_ = id;
            env->cfilterenv_reset(IN_AUDIO, id.time());
            return true;
        }

        bool cfilterfunc_process(piw::cfilterenv_t *env, unsigned long long from, unsigned long long to, unsigned long samplerate, unsigned buffersize);

        bool cfilterfunc_end(piw::cfilterenv_t *env, unsigned long long to)
        {
            return false;
        }

        piw::data_nb_t id_;
        unsigned long long clip_hold_stop_time;
    };
}

namespace vu_meter
{
    struct vu_meter_t::impl_t: piw::cfilterctl_t, piw::cfilter_t
    {
        impl_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain) : cfilter_t(this, output, domain) {}
        piw::cfilterfunc_t *cfilterctl_create(const piw::data_t &path) { return new vu_meter_func_t(path,this); }
        unsigned long long cfilterctl_thru() { return 0; }
        unsigned long long cfilterctl_inputs() { return IN_MASK; }
        unsigned long long cfilterctl_outputs() { return OUT_MASK; }
        pic::flipflop_t<segment_list_t> segments;
        float clip_hold;
    };

	vu_meter_t::vu_meter_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain) : impl_(new impl_t(output, domain)) {}
	piw::cookie_t vu_meter_t::cookie() { return impl_->cookie(); }
	vu_meter_t::~vu_meter_t() { delete impl_; }

	void vu_meter::vu_meter_t::set_parameters(float signal,float high,float clip,int number_of_segments, float clip_hold) {
		impl_->segments.alternate().clear();
		//remember dB's are -ve numbers
		float db_per_segment = signal/number_of_segments;

		//calculate display segment boundaries.
		//everything in dBs to keep from having to to a square root in RMS calculations
		for (int i = 0; i < number_of_segments; ++i) {
			segment_data_t segment;
			float top = db_per_segment * i;
			float bottom = db_per_segment * (i+1);
			segment.signal_level = bottom > high ? NOT_ON_SEGMENT : bottom;
			segment.high_level = top < high ? NOT_ON_SEGMENT : bottom > high ? bottom : high;
			segment.clip_level = top < clip ? NOT_ON_SEGMENT : bottom > clip ? bottom : clip;
			segment.index = number_of_segments - i;
			impl_->segments.alternate().push_back(segment);
		}
		impl_->segments.exchange();
		impl_->clip_hold = clip_hold * 1e6;
	}
}

bool vu_meter_func_t::cfilterfunc_process(piw::cfilterenv_t *env, unsigned long long from, unsigned long long to, unsigned long samplerate, unsigned buffersize)
{
	unsigned long long t = piw::tsd_time();
    unsigned signal;
    piw::data_nb_t value;

    //RMS calculation
    double signal_squared = 0;
    int num_signals = 0;
    while(env->cfilterenv_next(signal, value, to))
    {
    	num_signals ++;
    	const float* d = value.as_array();
		for (unsigned i=0; i < buffersize; i += 1)
			signal_squared += d[i]*d[i];
    }
    //as everything in dBs, no need for sqrt()
    //...cause log(sqrt(x)) == 0.5 log(x), RMS in dB is 20log(sqrt(x)), so = 10log(sum of x squared)
    double level = 10 * log( signal_squared / (buffersize*num_signals) );

    //convert to segment display values
    pic::flipflop_t<segment_list_t>::guard_t segments_g(impl_->segments);
	unsigned char *output_string;
	piw::data_nb_t light_out = piw::makeblob_nb(t,5*segments_g.value().size(),&output_string);

	for ( segment_list_t::const_iterator segment=segments_g.value().begin() ; segment != segments_g.value().end(); segment++ ) {
    	unsigned char* dp = output_string + (segment->index-1) * 5;
    	
    	unsigned char status = BLANK_REGION;
		if ( level > segment->clip_level  || ( segment->clip_level != NOT_ON_SEGMENT && clip_hold_stop_time > t ) ) {
			status = CLIP_REGION;
		    if (clip_hold_stop_time < t)
		    	clip_hold_stop_time = t + impl_->clip_hold;
		}
		else if ( level > segment->high_level) {
			status = HIGH_REGION;
		}
		else if ( level > segment->signal_level ) {
			status = SIGNAL_REGION;
		}
        piw::statusdata_t(false,piw::coordinate_t(1,segment->index),status).to_bytes(dp);
	}

	env->cfilterenv_output(OUT_LIGHT, light_out);

    return true;
}
