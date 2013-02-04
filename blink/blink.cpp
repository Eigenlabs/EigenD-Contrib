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

#include <piw/piw_data.h>
#include <piw/piw_keys.h>
#include <piw/piw_thing.h>
#include <piw/piw_cfilter.h>
#include <picross/pic_stl.h>

#include "blink.h"
#include "blink1-lib.h"

namespace
{
    struct blink1_t: piw::cfilterctl_t, piw::cfilter_t, virtual public pic::counted_t
    {
        blink1_t(blink::blink_t::impl_t *, unsigned, piw::clockdomain_ctl_t *);
        ~blink1_t();

        unsigned id() { return id_; }
        void refresh_dev();
        void set_color(float, float, float);
        void set_color_raw(float, float, float);
        void refresh_color();
        
        void show_red(float, unsigned);
        void show_green(float, unsigned);
        void show_blue(float, unsigned);

        piw::cfilterfunc_t *cfilterctl_create(const piw::data_t &);
        unsigned long long cfilterctl_thru() { return 0; }
        unsigned long long cfilterctl_inputs() { return SIG3(1,2,3); }
        unsigned long long cfilterctl_outputs() { return 0; }

        blink::blink_t::impl_t *root_;
        unsigned id_;
        hid_device *dev_;

        float last_red_;
        float last_green_;
        float last_blue_;
        
        float scheduled_red_;
        float scheduled_green_;
        float scheduled_blue_;
        
        unsigned long long last_timer_red_;
        unsigned long long last_timer_green_;
        unsigned long long last_timer_blue_;
    };
    
    struct func_t: piw::cfilterfunc_t
    {
        func_t(blink1_t *blink) : blink_(blink)
        {
        }

        bool cfilterfunc_start(piw::cfilterenv_t *env, const piw::data_nb_t &id)
        {
            env->cfilterenv_reset(1,id.time());
            env->cfilterenv_reset(2,id.time());
            env->cfilterenv_reset(3,id.time());

            process(env, piw::tsd_time());
            return true;
        }

        bool cfilterfunc_process(piw::cfilterenv_t *env, unsigned long long from, unsigned long long to, unsigned long samplerate, unsigned buffersize)
        {
            process(env, to);
            return true;
        }

        bool cfilterfunc_end(piw::cfilterenv_t *env, unsigned long long to)
        {
            process(env, to);
            return false;
        }

        void process(piw::cfilterenv_t *env, unsigned long long to)
        {
            bool color_changed = false;

            float red = blink_->last_red_;
            float green = blink_->last_green_;
            float blue = blink_->last_blue_;

            piw::data_nb_t d;
            if(env->cfilterenv_latest(1,d,to))
            {
                color_changed = true;
                red = d.as_renorm(0.f, 1.f, 0.f);
            }
            if(env->cfilterenv_latest(2,d,to))
            {
                color_changed = true;
                green = d.as_renorm(0.f, 1.f, 0.f);
            }
            if(env->cfilterenv_latest(3,d,to))
            {
                color_changed = true;
                blue = d.as_renorm(0.f, 1.f, 0.f);
            }
            
            if(blink_->last_timer_red_ > to)
            {
                color_changed = true;
                red = blink_->scheduled_red_;
            }
            if(blink_->last_timer_green_ > to)
            {
                color_changed = true;
                green = blink_->scheduled_green_;
            }
            if(blink_->last_timer_blue_ > to)
            {
                color_changed = true;
                blue = blink_->scheduled_blue_;
            }

            if(color_changed)
            {
                blink_->set_color(red, green, blue);
            }
        }

        blink1_t *blink_;
    };
    
};

struct blink::blink_t::impl_t: piw::thing_t
{
    impl_t(piw::clockdomain_ctl_t *);
    ~impl_t();
    piw::cookie_t create_blink(unsigned);
    void show_colour_data(const piw::data_nb_t &d);
    piw::change_nb_t show_colour();
    
    void thing_timer_slow();

    piw::clockdomain_ctl_t *domain_;
    pic::ref_t<blink1_t> blinks_[blink1_max_devices];
    int vid_;
    int pid_;
    int count_;
};


/**
 * blink1_t
 */

blink1_t::blink1_t(blink::blink_t::impl_t *root, unsigned id, piw::clockdomain_ctl_t *domain):
    cfilter_t(this, piw::cookie_t(0), domain), root_(root), id_(id), dev_(0),
    last_red_(-1.f), last_green_(-1.f), last_blue_(-1.f),
    scheduled_red_(-1.f), scheduled_green_(-1.f), scheduled_blue_(-1.f),
    last_timer_red_(0), last_timer_green_(0), last_timer_blue_(0)
{
    refresh_dev();
}

blink1_t::~blink1_t()
{
    if(dev_)
    {
        //blink1_close(dev_);
        dev_ = 0;
    }
}

void blink1_t::refresh_dev()
{
    if(dev_)
    {
        //blink1_close(dev_);
        dev_ = 0;
    }
    dev_ = blink1_openById(id_);
    if(dev_ == NULL)
    { 
        pic::logmsg() << "Can't open blink(1) " << id_;
    }
    else
    {
        set_color_raw(last_red_, last_green_, last_blue_);
    }
}


piw::cfilterfunc_t *blink1_t::cfilterctl_create(const piw::data_t &)
{
    return new func_t(this);
}

void blink1_t::refresh_color()
{
    if(last_red_>=0 && last_green_>=0 && last_blue_>=0)
    {
        set_color_raw(last_red_, last_green_, last_blue_);
    }
}

void blink1_t::set_color(float red, float green, float blue)
{
    if(red<0.f) red = 0.f;
    if(red>1.f) red = 1.f;
    if(green<0.f) green = 0.f;
    if(green>1.f) green = 1.f;
    if(blue<0.f) blue = 0.f;
    if(blue>1.f) blue = 1.f;
    
    if(red==last_red_ && green==last_green_ && blue==last_blue_)
    {
        return;
    }

    last_red_ = red;
    last_green_ = green;
    last_blue_ = blue;

    set_color_raw(red, green, blue);
}

void blink1_t::set_color_raw(float red, float green, float blue)
{
    if(dev_)
    {
        blink1_setRGB(dev_, red*255, green*255, blue*255);
    }
}

void blink1_t::show_red(float value, unsigned length)
{
    scheduled_red_ = value;
    last_timer_red_ = piw::tsd_time() + length;
}

void blink1_t::show_green(float value, unsigned length)
{
    scheduled_green_ = value;
    last_timer_green_ = piw::tsd_time() + length;
}

void blink1_t::show_blue(float value, unsigned length)
{
    scheduled_blue_ = value;
    last_timer_blue_ = piw::tsd_time() + length;
}


/**
 * blink::blink_t::impl_t
 */

blink::blink_t::impl_t::impl_t(piw::clockdomain_ctl_t *domain) : domain_(domain), count_(-1)
{
    piw::tsd_thing(this);
    vid_ = blink1_vid();
    pid_ = blink1_pid();
    thing_timer_slow();
}

blink::blink_t::impl_t::~impl_t()
{
}

piw::cookie_t blink::blink_t::impl_t::create_blink(unsigned index)
{
    if(index < 1 || index > blink1_max_devices)
    {
        return piw::cookie_t(0);
    }

    blink1_t *instance = new blink1_t(this, index-1, domain_);
    blinks_[index-1] = pic::ref(instance);
    return instance->cookie();
}

void blink::blink_t::impl_t::show_colour_data(const piw::data_nb_t &d)
{
    if(!d.is_tuple() || d.as_tuplelen() != 5)
    {
        return;
    }
    piw::data_nb_t index = d.as_tuple_value(0);
    piw::data_nb_t red = d.as_tuple_value(1);
    piw::data_nb_t green = d.as_tuple_value(2);
    piw::data_nb_t blue = d.as_tuple_value(3);
    piw::data_nb_t length = d.as_tuple_value(4);
    if(!index.is_long() ||
       (!red.is_null() && !red.is_float()) || 
       (!green.is_null() && !green.is_float()) ||
       (!blue.is_null() && !blue.is_float()) ||
       !length.is_long())
    {
        return;
    }
    
    long i = index.as_long()-1;
    long l = length.as_long();
    if(i < 0 || i >= blink1_max_devices || l <= 0 || !blinks_[i].isvalid())
    {
        return;
    }
    
    if(!red.is_null())
    {
        blinks_[i]->show_red(red.as_float(), l);
    }
    if(!green.is_null())
    {
        blinks_[i]->show_green(green.as_float(), l);
    }
    if(!blue.is_null())
    {
        blinks_[i]->show_blue(blue.as_float(), l);
    }
}

piw::change_nb_t blink::blink_t::impl_t::show_colour()
{
    return piw::change_nb_t::method(this, &blink::blink_t::impl_t::show_colour_data);
}

void blink::blink_t::impl_t::thing_timer_slow()
{
    int count = blink1_enumerateByVidPid(vid_,pid_);
    if(count != count_)
    {
        pic::logmsg() << count << " blink(1) devices found";
        for(int i = 0; i < blink1_max_devices; ++i)
        {
            if(blinks_[i].isvalid())
            {
                blinks_[i]->refresh_dev();
            }
        }
    }
    count_ = count;
    timer_slow(1000);
}

/**
 * blink::blink_t
 */

blink::blink_t::blink_t(piw::clockdomain_ctl_t *domain) : impl_(new impl_t(domain))
{
}

blink::blink_t::~blink_t()
{
    delete impl_;
}

piw::cookie_t blink::blink_t::create_blink(unsigned index)
{
    return impl_->create_blink(index);
}

piw::change_nb_t blink::blink_t::show_colour()
{
    return impl_->show_colour();
}
