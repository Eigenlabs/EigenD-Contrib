
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


#include "sample_and_hold.h"
#include <piw/piw_cfilter.h>
#include <piw/piw_clock.h>
#include <piw/piw_address.h>
#include <picross/pic_float.h>
#include <picross/pic_time.h>
#include <cmath>

#define IN_SOURCE 1
#define IN_GATE 2
#define IN_MASK SIG2(IN_SOURCE, IN_GATE)

#define OUT_AUDIO 1
#define OUT_MASK SIG1(OUT_AUDIO)


namespace 
{
    struct sample_and_holdfunc_t: piw::cfilterfunc_t
    {
        sample_and_holdfunc_t() : current_output_(0), current_gate_(-1.0f)
        {
        }

        bool cfilterfunc_start(piw::cfilterenv_t *env, const piw::data_nb_t &id)
        {
            unsigned long long t=id.time();
            env->cfilterenv_reset(IN_SOURCE,t);
            env->cfilterenv_reset(IN_GATE,t);

            return true;
        }

        bool cfilterfunc_process(piw::cfilterenv_t *env, unsigned long long from, unsigned long long to,
            unsigned long sample_rate, unsigned buffer_size)
        {
            float *buffer_out, *scalar;
            
            piw::data_nb_t audio_out = piw::makenorm_nb(to ,buffer_size, &buffer_out, &scalar);

            // necessary??
            memset(buffer_out, 0, buffer_size*sizeof(float));
            *scalar = 0;

            piw::data_nb_t audio_value;

            const float *audio_in=0;
            unsigned int audio_in_len = 0;
            const float *gate_in=0;
            unsigned int gate_in_len = 0;
            if(env->cfilterenv_nextsig(IN_SOURCE, audio_value, to))
            {
                piw::data_nb_t gate_value;;
                audio_in = audio_value.as_array();
                audio_in_len = audio_value.as_arraylen();

                if(env->cfilterenv_nextsig(IN_GATE, gate_value, to))
                {
                    gate_in = gate_value.as_array();
                    gate_in_len = gate_value.as_arraylen();
            pic::logmsg() << "audio in len " << audio_in_len;
            pic::logmsg() << "gate in len " << gate_in_len;
                    for (unsigned int c =0; c < audio_in_len; c++) {
                        if (gate_in[c] == 0.0f && current_gate_ < 0.0f) {
                            current_output_ = audio_in[c];
            pic::logmsg() << "sample zero gate" << current_output_;
                        }
                        else if (current_gate_/gate_in[c] < 0.0 && current_gate_ < 0.0f) {
                            current_output_ = audio_in[c];
            pic::logmsg() << "sample trans " << current_output_;
                        } 
                        buffer_out[c] = current_output_;
                        current_gate_ = gate_in[c];
                    }
                } else {
                    for (unsigned int c =0; c < audio_in_len; c++) {
                        buffer_out[c] = current_output_;
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

        float current_output_;
        float current_gate_;
    };

}


namespace sample_and_hold
{
    struct sample_and_hold_t::impl_t: piw::cfilterctl_t, piw::cfilter_t
    {
        impl_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain) : cfilter_t(this, output, domain) {}
        piw::cfilterfunc_t *cfilterctl_create(const piw::data_t &path) { return new sample_and_holdfunc_t(); }

        unsigned long long cfilterctl_thru() { return 0; }
        unsigned long long cfilterctl_inputs() { return IN_MASK; }
        unsigned long long cfilterctl_outputs() { return OUT_MASK; }
    };

    sample_and_hold_t::sample_and_hold_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain) : 
            impl_(new impl_t(output, domain)) {}
    piw::cookie_t sample_and_hold_t::cookie() { return impl_->cookie(); }
    sample_and_hold_t::~sample_and_hold_t() { delete impl_; }

}


