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

#include "vu_meter.h"

#define IN_AUDIO 11
#define IN_MASK SIG1(IN_AUDIO)

#define OUT_LIGHT 56
#define OUT_MASK SIG1(OUT_LIGHT)

#define LED_STRUCT_SIZE

namespace
{
	struct vu_meter_key_t
	{
		float signal_level;
		float high_level;
		float clip_level;
	};

    struct vu_meter_func_t: piw::cfilterfunc_t
    {
    	const vu_meter::vu_meter_t::impl_t* impl_;

        vu_meter_func_t(const piw::data_t &path, const vu_meter::vu_meter_t::impl_t* impl)
        {
        	impl_ = impl;
            pic::logmsg() << "create " << path;
        }

        bool cfilterfunc_start(piw::cfilterenv_t *env, const piw::data_nb_t &id)
        {
            pic::logmsg() << "start " << id;

            id_ = id;

            env->cfilterenv_reset(IN_AUDIO, id.time());

            return true;
        }

        bool cfilterfunc_process(piw::cfilterenv_t *env, unsigned long long from, unsigned long long to, unsigned long samplerate, unsigned buffersize);
        bool cfilterfunc_end(piw::cfilterenv_t *env, unsigned long long to)
        {
            pic::logmsg() << "end " << id_;

            return false;
        }

        piw::data_nb_t id_;
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
        pic::flipflop_t<vu_meter_key_t> key;
        int size;
    };

	vu_meter_t::vu_meter_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain) : impl_(new impl_t(output, domain)) {}
	piw::cookie_t vu_meter_t::cookie() { return impl_->cookie(); }
	vu_meter_t::~vu_meter_t() { delete impl_; }

	void vu_meter::vu_meter_t::set_signal_level(float value)
	{
		float v = pic::approx::pow(10.0,value/20.0);
		impl_->key.alternate().signal_level = v;
		impl_->key.exchange();
		pic::logmsg() << "set signal level to " << v;
	}

	void vu_meter::vu_meter_t::set_high_level(float value)
	{
		float v = pic::approx::pow(10.0,value/20.0);
		impl_->key.alternate().high_level = v;
		impl_->key.exchange();
		pic::logmsg() << "set high level to " << v;
	}

	void vu_meter::vu_meter_t::set_clip_level(float value)
	{
		float v = pic::approx::pow(10.0,value/20.0);
		impl_->key.alternate().clip_level = v;
		impl_->key.exchange();
		pic::logmsg() << "set clip level to " << v;
	}

	void vu_meter::vu_meter_t::set_size(int value)
	{
		impl_->size = value;
		pic::logmsg() << "set size to " << value;
	}

}

bool vu_meter_func_t::cfilterfunc_process(piw::cfilterenv_t *env, unsigned long long from, unsigned long long to, unsigned long samplerate, unsigned buffersize)
{
	unsigned long long t = piw::tsd_time();



    unsigned signal;
    piw::data_nb_t value;
    float level = 0;
    while(env->cfilterenv_next(signal, value, to))
    {
        switch(signal)
        {
            case IN_AUDIO:
				const float* d = value.as_array();
            	for (unsigned i=0; i < buffersize; i += 1)
            		if (level < d[i])
            			level = d[i];
            	break;
        }
    }

    int lit_keys = level * impl_->size;

	unsigned char *output_string;
	piw::data_nb_t light_out = piw::makeblob_nb(t,5*lit_keys,&output_string);
    for (int key = 0; key < lit_keys; ++key) {
    	unsigned char* dp = output_string + key*5;
		piw::statusdata_t::int2c(1,dp+0);
		piw::statusdata_t::int2c(1+key,dp+2);
		{
			pic::flipflop_t<vu_meter_key_t>::guard_t g(impl_->key);

			pic::logmsg() << "level " << level << "s "<< g.value().signal_level << "h "<< g.value().high_level << "c "<< g.value().clip_level;
			piw::statusdata_t::status2c(false,
					(level < g.value().signal_level) ? BCTSTATUS_OFF :
					(level < g.value().high_level) ? BCTSTATUS_ACTIVE :
					(level < g.value().clip_level) ? BCTSTATUS_UNKNOWN :
							BCTSTATUS_SELECTOR_OFF ,dp+4);
		}

		env->cfilterenv_output(OUT_LIGHT, light_out);
    }

    return true;
}
