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
#include <picross/pic_stl.h>

#include "cv_calibration.h"

#define IN_FREQUENCY 1
#define IN_MASK SIG1(IN_FREQUENCY)

#define OUT_VOLTAGE 1
#define OUT_CALIBRATION 2
#define OUT_MASK SIG2(OUT_VOLTAGE,OUT_CALIBRATION)

#define CALIBRATION_RESOLUTION 100
#define CALIBRATION_TIMEOUT 10000000

#define CV_CALIBRATION_DEBUG 1 

namespace
{
    typedef pic::lckmap_t<float,float>::nbtype calibration_map_t;

    class cv_calibrator_t
    {
        public:
            cv_calibrator_t() : calibrate_(false), calibrating_(false), calibration_step_(0), last_timestamp_(0), last_voltage_(-1.0f), waiting_(false)
            {
            }

            static inline void output_voltage(piw::cfilterenv_t *env, float v, unsigned long long t)
            {
                env->cfilterenv_output(OUT_VOLTAGE, piw::makefloat_bounded_nb(1.0f,-1.0f,0.0f,v,t));
            }

            static inline void output_calibration(piw::cfilterenv_t *env, bool v, unsigned long long t)
            {
                env->cfilterenv_output(OUT_CALIBRATION, piw::makebool_nb(v,t));
            }

            void toggle_calibration(bool v) { calibrate_ = v; }

            bool recalibrate(piw::cfilterenv_t *env, unsigned long long from, unsigned long long to, unsigned buffersize)
            {
                if(calibrate_)
                {
                    // start from a clean slate
                    if(!calibrating_)
                    {
#if CV_CALIBRATION_DEBUG>0
                        pic::logmsg() << "starting new calibration";
#endif // CV_CALIBRATION_DEBUG>0

                        // set voltage to 0 and close the gate to start
                        output_voltage(env, -1.0f, to);
                        output_calibration(env, false, to);

                        env->cfilterenv_reset(IN_FREQUENCY, from);

                        calibrating_ = true;
                        last_timestamp_ = to;
                        calibration_data_.clear();
                    }

                    // stop the calibration when the required resolution has been achieved
                    if(calibration_step_>CALIBRATION_RESOLUTION)
                    {
#if CV_CALIBRATION_DEBUG>0
                        pic::logmsg() << "stopping calibration, it reached resolution " << CALIBRATION_RESOLUTION;
#endif // CV_CALIBRATION_DEBUG>0

                        stop_calibration(env, to);
                    }
                    // interrupt the calibration when it has taken too long since the last step
                    else if((to - last_timestamp_) > CALIBRATION_TIMEOUT)
                    {
#if CV_CALIBRATION_DEBUG>0
                        pic::logmsg() << "calibration timeout at step " << calibration_step_;
#endif // CV_CALIBRATION_DEBUG>0

                        calibration_data_.clear();

                        stop_calibration(env, to);
                    }
                    // process the individual calibration steps
                    else
                    {
                        // the first calibration phase which waits for a zero frequency,
                        // after which it sends out a voltage and opens the gate
                        if(!waiting_)
                        {
                            piw::data_nb_t value;
                            if(env->cfilterenv_nextsig(IN_FREQUENCY, value, to))
                            {
                                float frequency = value.as_float();

                                if(0.0f == frequency)
                                {
                                    last_voltage_ = float(calibration_step_)/(CALIBRATION_RESOLUTION/2)-1.f;

#if CV_CALIBRATION_DEBUG>0
                                    pic::logmsg() << "calibration step " << calibration_step_ << " got zero frequency, sending voltage " << last_voltage_;
#endif // CV_CALIBRATION_DEBUG>0

                                    output_voltage(env, last_voltage_, to);
                                    output_calibration(env, true, to);
                                    waiting_ = true;
                                    last_timestamp_ = to;
                                }
                            }
                        }
                        // the second calibration phase waits for the frequency to arrive that
                        // corresponds to the voltage that was sent out in the previous phase,
                        // after which it closes the gate, stores the calibration data
                        // and increases the resolution
                        else
                        {
                            piw::data_nb_t value;
                            if(env->cfilterenv_nextsig(IN_FREQUENCY, value, to))
                            {
                                float frequency = value.as_float();
                                calibration_data_.insert(std::pair<float,float>(frequency,last_voltage_));

#if CV_CALIBRATION_DEBUG>0
                                pic::logmsg() << "calibration step " << calibration_step_ << " got frequency " << frequency;
#endif // CV_CALIBRATION_DEBUG>0


                                output_calibration(env, false, to);
                                waiting_ = false;

                                ++calibration_step_;
                                last_timestamp_ = to;
                            }
                        }
                    }
                }
                else
                {
                    // properly clean up in case the calibration was turned off
                    // before all the steps were completed
                    if(calibration_step_>0)
                    {
                        calibration_data_.clear();

                        stop_calibration(env, to);
                    }
                }

                return calibrate_;
            }

            float get_voltage(float frequency)
            {
                if(!frequency || calibrating_)
                {
                    return -1.0f;
                }

                float last_freq = 0.0f;
                float last_volt = -1.0f;

                calibration_map_t::iterator it;
                for(it = calibration_data_.begin(); it != calibration_data_.end(); it++)
                {
                    float freq = it->first;
                    float volt = it->second;

                    if(freq > frequency && last_freq < freq)
                    {
                        float baseline = last_volt;
                        float max_offset = volt-last_volt;
                        float numerator = frequency-last_freq;
                        float denominator = freq-last_freq;
                        float fraction = numerator/denominator;
                        float voltage = baseline + max_offset * fraction; //last_volt+(volt-last_volt)*(frequency-last_freq/freq-last_freq);
                        pic::logmsg() << "baseline=" << baseline << " max_offset=" << max_offset << " numerator=" << numerator << " denominator=" << denominator << " fraction=" << fraction << " voltage=" << voltage;

#if CV_CALIBRATION_DEBUG>0
                        pic::logmsg() << "voltage lookup for frequency " << frequency << " : result " << voltage << " (" << last_freq << "," << freq << "," << last_volt << "," << volt << ")";
#endif // CV_CALIBRATION_DEBUG>0

                        return voltage;
                    }

                    last_freq = freq;
                    last_volt = volt;
                }

                return -1.0f;
            }

        private:

            void stop_calibration(piw::cfilterenv_t *env, unsigned long long to)
            {
#if CV_CALIBRATION_DEBUG>0
                pic::logmsg() << "clearing finished calibration state";
#endif // CV_CALIBRATION_DEBUG>0

                calibrate_ = false;
                calibrating_ = false;
                calibration_step_ = 0;
                last_timestamp_ = 0;
                last_voltage_ = -1.0f;
                waiting_ = false;

                output_voltage(env, -1.0f, to);
                output_calibration(env, false, to);
            }

            calibration_map_t calibration_data_;

            bool calibrate_;

            bool calibrating_;
            unsigned calibration_step_;
            unsigned long long last_timestamp_;
            float last_voltage_;
            bool waiting_;
    };
}

namespace cv_calibration
{
    struct cv_calibration_t::impl_t: piw::cfilterctl_t, piw::cfilter_t
    {
        impl_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain) : cfilter_t(this, output, domain) {}
        piw::cfilterfunc_t *cfilterctl_create(const piw::data_t &path);
        unsigned long long cfilterctl_thru() { return 0; }
        unsigned long long cfilterctl_inputs() { return IN_MASK; }
        unsigned long long cfilterctl_outputs() { return OUT_MASK; }

        void calibrate(bool v) { calibrator_.toggle_calibration(v); }

        cv_calibrator_t calibrator_;
    };
}

namespace
{
    struct cv_calibration_func_t: piw::cfilterfunc_t
    {
        cv_calibration_func_t(cv_calibration::cv_calibration_t::impl_t *root) : root_(root)
        {
        }

        bool cfilterfunc_start(piw::cfilterenv_t *env, const piw::data_nb_t &id)
        {
            id_ = id;

            env->cfilterenv_reset(IN_FREQUENCY, id.time());

            cv_calibrator_t::output_voltage(env, -1.0f, id.time());
            cv_calibrator_t::output_calibration(env, false, id.time());

            return true;
        }


        bool cfilterfunc_process(piw::cfilterenv_t *env, unsigned long long from, unsigned long long to, unsigned long samplerate, unsigned buffersize)
        {
            if(!root_->calibrator_.recalibrate(env, from, to, buffersize))
            {
                piw::data_nb_t value;
                if(env->cfilterenv_latest(IN_FREQUENCY, value, to))
                {
                    cv_calibrator_t::output_voltage(env, root_->calibrator_.get_voltage(value.as_float()), piw::tsd_time());
                }
            }

            return true;
        }

        bool cfilterfunc_end(piw::cfilterenv_t *env, unsigned long long to)
        {
            return false;
        }

        cv_calibration::cv_calibration_t::impl_t *root_;

        piw::data_nb_t id_;
    };
}

piw::cfilterfunc_t * cv_calibration::cv_calibration_t::impl_t::cfilterctl_create(const piw::data_t &path) { return new cv_calibration_func_t(this); }

cv_calibration::cv_calibration_t::cv_calibration_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain) : impl_(new impl_t(output, domain)) {}
piw::cookie_t cv_calibration::cv_calibration_t::cookie() { return impl_->cookie(); }
cv_calibration::cv_calibration_t::~cv_calibration_t() { delete impl_; }

static int __calibrate(void *i_, void *v_)
{
    cv_calibration::cv_calibration_t::impl_t *i = (cv_calibration::cv_calibration_t::impl_t *)i_;
    bool v = *(bool *)v_;
    i->calibrate(v);
    return 0;
}

void cv_calibration::cv_calibration_t::calibrate()
{
    bool flag = true;
    piw::tsd_fastcall(__calibrate,impl_,(void *)&flag);
}


