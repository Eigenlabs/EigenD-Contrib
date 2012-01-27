
/*
 Copyright 2009 Eigenlabs Ltd.  http://www.eigenlabs.com

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

#include <picross/pic_config.h>


#include "synth_noise.h"
#include "OnePole.h"
#include <piw/piw_cfilter.h>
#include <piw/piw_clock.h>
#include <piw/piw_address.h>
#include <picross/pic_float.h>
#include <picross/pic_time.h>
#include <cmath>

#define IN_VOL 1
#define IN_FILTER_FREQ 2
#define IN_MASK SIG2(IN_VOL, IN_FILTER_FREQ)

#define OUT_AUDIO 1
#define OUT_MASK SIG1(OUT_AUDIO)

#define DEFAULT_VOLUME 0.0f
#define DEFAULT_FILTER_FREQ 20000.0f

namespace 
{
    struct noisefunc_t: piw::cfilterfunc_t
    {
        noisefunc_t() : current_volume_(DEFAULT_VOLUME),
            current_filter_freq_(DEFAULT_FILTER_FREQ),
            filter_()
        {}

        StkFloat local_rand() 
        {
            return (StkFloat)(((rand()/((StkFloat)RAND_MAX))-0.5f)*2.0f);
        }

        bool cfilterfunc_start(piw::cfilterenv_t *env, const piw::data_nb_t &id)
        {
            unsigned long long t=id.time();
            env->cfilterenv_reset(IN_VOL,t);
            env->cfilterenv_reset(IN_FILTER_FREQ,t);

            filter_.clear();
            filter_.setPole(current_filter_freq_);

            return true;
        }

        bool cfilterfunc_process(piw::cfilterenv_t *env, 
            unsigned long long from, unsigned long long to,
            unsigned long sample_rate, unsigned buffer_size)
        {
            piw::data_nb_t vol_value;
            piw::data_nb_t filter_freq_value;
            float *buffer_out, *scalar;
            unsigned int buffer_in_len = 0;
            const float *vol_in=0;
            const float *freq_in=0;
            bool vol_changed;
            bool freq_changed;
            
            piw::data_nb_t audio_out = piw::makenorm_nb(to ,buffer_size, &buffer_out, &scalar);

            memset(buffer_out, 0, buffer_size*sizeof(float));
            *scalar = 0;


            vol_changed = env->cfilterenv_nextsig(IN_VOL, vol_value, to);
            freq_changed = env->cfilterenv_nextsig(IN_FILTER_FREQ, filter_freq_value, to);

            if(vol_changed) {
                vol_in = vol_value.as_array();
                buffer_in_len = vol_value.as_arraylen();
                current_volume_ = vol_value.as_renorm(0, 1, 0);
                if(freq_changed) {
                    freq_in = filter_freq_value.as_array();
                    current_filter_freq_ = filter_freq_value.as_renorm(0.0f, 20000.0f, 20000.0f);
                    for (unsigned int c =0; c < buffer_in_len; c++) {
                        filter_.setPole(freq_in[c]);
                        buffer_out[c] = vol_in[c] * filter_.tick(local_rand());
                    }
                } else {
                    for (unsigned int c =0; c < buffer_in_len; c++) {
                        buffer_out[c] = vol_in[c] * filter_.tick(local_rand());
                    }
                }

            }  else {
                if(freq_changed) {    
                    freq_in = filter_freq_value.as_array();
                    buffer_in_len = filter_freq_value.as_arraylen();
                    current_filter_freq_ = filter_freq_value.as_renorm(0.0f, 20000.0f, 20000.0f);
                    for (unsigned int c =0; c < buffer_in_len; c++) {
                        filter_.setPole( freq_in[c] );
                        buffer_out[c] = current_volume_ * filter_.tick(local_rand());
                    }
                } else {
                    for (unsigned int c =0; c < buffer_size; c++) {
                        buffer_out[c] = current_volume_ * filter_.tick(local_rand());
                    }
                }
            }
            *scalar = buffer_out[buffer_size - 1];
            env->cfilterenv_output(OUT_AUDIO, audio_out);
            return true;
        }

        bool cfilterfunc_end(piw::cfilterenv_t *env, unsigned long long t)
        {
            return false;
        }

        float current_volume_, current_filter_freq_;
        float count_;
        OnePole filter_;
    };
}


namespace synth_noise
{
    struct noise_t::impl_t: piw::cfilterctl_t, piw::cfilter_t
    {
        impl_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain) : cfilter_t(this, output, domain) {}
        piw::cfilterfunc_t *cfilterctl_create(const piw::data_t &path) { return new noisefunc_t(); }
        unsigned long long cfilterctl_thru() { return 0; }
        unsigned long long cfilterctl_inputs() { return IN_MASK; }
        unsigned long long cfilterctl_outputs() { return OUT_MASK; }
    };

    noise_t::noise_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain) : impl_(new impl_t(output, domain)) {}
    piw::cookie_t noise_t::cookie() { return impl_->cookie(); }
    noise_t::~noise_t() { delete impl_; }

}


