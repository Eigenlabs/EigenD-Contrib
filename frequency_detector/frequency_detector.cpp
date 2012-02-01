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
#include <cmath>

#include "frequency_detector.h"

#define IN_AUDIO 1
#define IN_MASK SIG1(IN_AUDIO)

#define OUT_FREQUENCY 1
#define OUT_MASK SIG1(OUT_FREQUENCY)

#define DEFAULT_THRESHOLD 0.01f
#define DEFAULT_BUFFER_COUNT 10 

#define FREQUENCY_DETECTOR_DEBUG 0 

namespace frequency_detector
{
    struct frequency_detector_t::impl_t: piw::cfilterctl_t, piw::cfilter_t
    {
        impl_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain) : cfilter_t(this, output, domain), threshold_(DEFAULT_THRESHOLD), buffer_count_(DEFAULT_BUFFER_COUNT) {}
        piw::cfilterfunc_t *cfilterctl_create(const piw::data_t &path);
        unsigned long long cfilterctl_thru() { return 0; }
        unsigned long long cfilterctl_inputs() { return IN_MASK; }
        unsigned long long cfilterctl_outputs() { return OUT_MASK; }

        float threshold_;
        unsigned buffer_count_;
    };
}

namespace
{
    struct frequency_detector_func_t: piw::cfilterfunc_t
    {
        frequency_detector_func_t(const frequency_detector::frequency_detector_t::impl_t *root) : root_(root)
        {
        }

        bool cfilterfunc_start(piw::cfilterenv_t *env, const piw::data_nb_t &id)
        {
            id_ = id;
            previous_sign_ = 0;
            measured_buffers_ = 0;
            zero_crossings_ = 0;
            first_crossing_sample_ = -1;
            last_crossing_sample_ = -1;

            env->cfilterenv_reset(IN_AUDIO, id.time());

            env->cfilterenv_output(OUT_FREQUENCY,piw::makefloat_bounded_units_nb(BCTUNIT_HZ,96000,0,0,0,id.time()));

            return true;
        }

        static inline int sign(float value)
        {
            return (value > 0) - (value < 0);
        }

        bool cfilterfunc_process(piw::cfilterenv_t *env, unsigned long long from, unsigned long long to, unsigned long samplerate, unsigned buffersize)
        {
            const float *audio_in = 0;
            piw::data_nb_t value;
            if(env->cfilterenv_nextsig(IN_AUDIO, value, to))
            {
                audio_in = value.as_array();
            }

            if(audio_in)
            {
                for(unsigned i = 0; i < buffersize; ++i)
                {
                    // use a rudimentary gate based on a sample's magnitude to start measuring
                    // this prevents very low audio signals to start the detection
                    if(!zero_crossings_ && fabsf(audio_in[i]) < root_->threshold_) continue;

                    int sgn = sign(audio_in[i]);
                    if(previous_sign_ != sgn)
                    {
                        ++zero_crossings_;
                        last_crossing_sample_ = measured_buffers_*buffersize+i;
                        if(-1==first_crossing_sample_)
                        {
                            first_crossing_sample_ = last_crossing_sample_;
                        }
                    }

                    previous_sign_ = sgn;
                }
            }
            ++measured_buffers_;

            if(measured_buffers_ == root_->buffer_count_)
            {
                if(zero_crossings_>1)
                {

                    unsigned total_samples = last_crossing_sample_-first_crossing_sample_;
                    float samples_per_crossing = float(total_samples)/(zero_crossings_-1);
                    float frequency = samplerate/(samples_per_crossing*2);
#if FREQUENCY_DETECTOR_DEBUG>0
                    pic::logmsg() << "crossings=" << zero_crossings_ << " first=" << first_crossing_sample_ << " last=" << last_crossing_sample_ << " total=" << total_samples << " frequency=" << frequency;
#endif // FREQUENCY_DETECTOR_DEBUG>0

                    env->cfilterenv_output(OUT_FREQUENCY,piw::makefloat_bounded_units_nb(BCTUNIT_HZ,96000,0,0,frequency,to));
                }
                else
                {
                    env->cfilterenv_output(OUT_FREQUENCY,piw::makefloat_bounded_units_nb(BCTUNIT_HZ,96000,0,0,0,to));
                }

                measured_buffers_ = 0;
                zero_crossings_ = 0;
                first_crossing_sample_ = -1;
                last_crossing_sample_ = -1;
            }

            return true;
        }

        bool cfilterfunc_end(piw::cfilterenv_t *env, unsigned long long to)
        {
            return false;
        }

        const frequency_detector::frequency_detector_t::impl_t *root_;

        piw::data_nb_t id_;
        int previous_sign_;
        unsigned measured_buffers_;
        unsigned zero_crossings_;
        int first_crossing_sample_;
        int last_crossing_sample_;
    };
}

piw::cfilterfunc_t * frequency_detector::frequency_detector_t::impl_t::cfilterctl_create(const piw::data_t &path) { return new frequency_detector_func_t(this); }

frequency_detector::frequency_detector_t::frequency_detector_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain) : impl_(new impl_t(output, domain)) {}
piw::cookie_t frequency_detector::frequency_detector_t::cookie() { return impl_->cookie(); }
frequency_detector::frequency_detector_t::~frequency_detector_t() { delete impl_; }

static int __set_threshold(void *i_, void *v_)
{
    frequency_detector::frequency_detector_t::impl_t *i = (frequency_detector::frequency_detector_t::impl_t *)i_;
    float v = *(float *)v_;
    i->threshold_ = v;
    return 0;
}

static int __set_buffer_count(void *i_, void *v_)
{
    frequency_detector::frequency_detector_t::impl_t *i = (frequency_detector::frequency_detector_t::impl_t *)i_;
    unsigned v = *(unsigned *)v_;
    i->buffer_count_ = v;
    return 0;
}

void frequency_detector::frequency_detector_t::set_threshold(float v)
{
    v = fabsf(v);
    piw::tsd_fastcall(__set_threshold,impl_,&v);
}

void frequency_detector::frequency_detector_t::set_buffer_count(unsigned v)
{
    if(v<5) return;
    piw::tsd_fastcall(__set_buffer_count,impl_,&v);
}

