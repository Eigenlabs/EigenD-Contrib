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

#define LED_STRUCT_SIZE

#define BLANK_REGION BCTSTATUS_OFF
#define SIGNAL_REGION BCTSTATUS_ACTIVE
#define HIGH_REGION BCTSTATUS_UNKNOWN
#define CLIP_REGION BCTSTATUS_SELECTOR_OFF

//have to use math.pow - pic::approx::pow not valid for large values
#define dBToLinear(x) pow(10.0,x/20.0)
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

		//calculate display segment boundaries. Convert from dB to linear values so
		//slow log/exp/pow clacs not done in fast thread
		for (int i = 0; i < number_of_segments; ++i) {
			segment_data_t segment;
			float top = db_per_segment * i;
			float bottom = db_per_segment * (i+1);
			segment.signal_level = bottom > high ? NOT_ON_SEGMENT : dBToLinear(bottom);
			segment.high_level = top < high ? NOT_ON_SEGMENT : bottom > high ? NOT_ON_SEGMENT : dBToLinear(high);
			segment.clip_level = top < clip ? NOT_ON_SEGMENT : bottom > clip ? NOT_ON_SEGMENT : dBToLinear(clip);
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

    //just a simple peak value
    float level = 0;
    while(env->cfilterenv_next(signal, value, to))
    {
    	const float* d = value.as_array();
		for (unsigned i=0; i < buffersize; i += 1)
			if (level < d[i])
				level = d[i];
    }

    //convert to segment display values
    pic::flipflop_t<segment_list_t>::guard_t segments_g(impl_->segments);
	unsigned char *output_string;
	piw::data_nb_t light_out = piw::makeblob_nb(t,5*segments_g.value().size(),&output_string);
	for ( segment_list_t::const_iterator segment=segments_g.value().begin() ; segment != segments_g.value().end(); segment++ ) {
    	unsigned char* dp = output_string + (segment->index-1) * 5;
		piw::statusdata_t::int2c(1,dp);
		piw::statusdata_t::int2c(segment->index,dp+2);
		if ( level > segment->clip_level  || ( segment->clip_level != NOT_ON_SEGMENT && clip_hold_stop_time > t ) ) {
		    piw::statusdata_t::status2c(false, CLIP_REGION ,dp+4);
		    if (clip_hold_stop_time < t)
		    	clip_hold_stop_time = t + impl_->clip_hold;
		}
		else if ( level > segment->high_level) {
			piw::statusdata_t::status2c(false, HIGH_REGION ,dp+4);
		}
		else if ( level > segment->signal_level ) {
			piw::statusdata_t::status2c(false, SIGNAL_REGION ,dp+4);
		}
		else {
			piw::statusdata_t::status2c(false, BLANK_REGION ,dp+4);
		}
	}

	env->cfilterenv_output(OUT_LIGHT, light_out);

    return true;
}
