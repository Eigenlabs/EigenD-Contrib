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
#include <piw/piw_tsd.h>

#include "sysin_events.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>

#define IN_MOUSE_X 1
#define IN_MOUSE_Y 2
#define IN_MASK SIG2(IN_MOUSE_X,IN_MOUSE_Y)

#define SYSIN_EVENTS_DEBUG 0 

namespace sysin_events
{
    struct sysin_events_t::impl_t: piw::cfilterctl_t, piw::cfilter_t, public pic::nocopy_t, virtual public pic::tracked_t
    {
        impl_t(piw::clockdomain_ctl_t *domain) : cfilter_t(this, 0, domain), mouse_x_scale_(10), mouse_y_scale_(10), mouse_x_deadband_(0.3), mouse_y_deadband_(0.4) {}
        piw::cfilterfunc_t *cfilterctl_create(const piw::data_t &path);
        unsigned long long cfilterctl_thru() { return 0; }
        unsigned long long cfilterctl_inputs() { return IN_MASK; }
        unsigned long long cfilterctl_outputs() { return 0; }

        static int __press_key_code(void *r_, void *k_)
        {
            unsigned k = *(unsigned *)k_;
            CGEventRef e = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)k, true);  
            CGEventPost(kCGSessionEventTap, e);  
            CFRelease(e);  

            return 0;
        }

        static int __press_key_char(void *r_, void *k_)
        {
            const char *k = *(const char **)k_;
            NSString *str = [[NSString alloc] initWithUTF8String:k];
            UniChar chr = [str characterAtIndex:0];
            CGEventRef e = CGEventCreateKeyboardEvent(NULL, 0, true);  
            CGEventKeyboardSetUnicodeString(e, 1, &chr);   
            CGEventPost(kCGSessionEventTap, e);  
            CFRelease(e);
            CFRelease(str);

            return 0;
        }

        void press_key_data(const piw::data_nb_t &d)
        {
            if(d.is_long())
            {
                unsigned code = d.as_long();
                piw::tsd_fastcall(__press_key_code,this,&code);
            }
            else if(d.is_string() && d.as_stringlen() > 0)
            {
                const char* data = d.as_string();
                piw::tsd_fastcall(__press_key_char,this,&data);
            }
        }

        piw::change_nb_t press_key() { return piw::change_nb_t::method(this,&sysin_events_t::impl_t::press_key_data); }

        static int __set_mouse_x_scale(void *i_, void *v_)
        {
            sysin_events_t::sysin_events_t::impl_t *i = (sysin_events_t::sysin_events_t::impl_t *)i_;
            unsigned v = *(unsigned *)v_;
            i->mouse_x_scale_ = v;
            return 0;
        }

        static int __set_mouse_y_scale(void *i_, void *v_)
        {
            sysin_events_t::sysin_events_t::impl_t *i = (sysin_events_t::sysin_events_t::impl_t *)i_;
            unsigned v = *(unsigned *)v_;
            i->mouse_y_scale_ = v;
            return 0;
        }

        static int __set_mouse_x_deadband(void *i_, void *v_)
        {
            sysin_events_t::sysin_events_t::impl_t *i = (sysin_events_t::sysin_events_t::impl_t *)i_;
            float v = *(float *)v_;
            i->mouse_x_deadband_ = v;
            return 0;
        }

        static int __set_mouse_y_deadband(void *i_, void *v_)
        {
            sysin_events_t::sysin_events_t::impl_t *i = (sysin_events_t::sysin_events_t::impl_t *)i_;
            float v = *(float *)v_;
            i->mouse_y_deadband_ = v;
            return 0;
        }
        
        unsigned mouse_x_scale_;
        unsigned mouse_y_scale_;
        float mouse_x_deadband_;
        float mouse_y_deadband_;
    };
}

namespace
{
    struct sysin_events_func_t: piw::cfilterfunc_t
    {
        sysin_events_func_t(const sysin_events::sysin_events_t::impl_t *root) : root_(root)
        {
        }

        bool cfilterfunc_start(piw::cfilterenv_t *env, const piw::data_nb_t &id)
        {
            id_ = id;

            env->cfilterenv_reset(IN_MOUSE_X, id.time());
            env->cfilterenv_reset(IN_MOUSE_Y, id.time());

            return true;
        }

        bool cfilterfunc_process(piw::cfilterenv_t *env, unsigned long long from, unsigned long long to, unsigned long samplerate, unsigned buffersize)
        {
            unsigned i;
            piw::data_nb_t d;

            CGEventRef e1 = CGEventCreate(NULL);
            CGPoint point = CGEventGetLocation(e1);

            bool moved = false;

            while(env->cfilterenv_next(i, d, to))
            {
                if(d.time() < from) continue;

                switch(i)
                {
                    case IN_MOUSE_X:
                        {
                            float v = d.as_norm();
                            if(fabs(v) >= root_->mouse_x_deadband_)
                            {
                                v = v - (sign(v) * root_->mouse_x_deadband_);
                                point.x -= v*root_->mouse_x_scale_;
                                moved = true;
                            }
                        }
                        break;
                    case IN_MOUSE_Y:
                        {
                            float v = d.as_norm();
                            if(fabs(v) >= root_->mouse_y_deadband_)
                            {
                                v = v - (sign(v) * root_->mouse_y_deadband_);
                                point.y += v*root_->mouse_y_scale_;
                                moved = true;
                            }
                        }
                        break;
                }
            }

            if(moved && point.x >= 0 && point.y >= 0)
            {
                CGEventRef e2 = CGEventCreateMouseEvent(NULL, kCGEventMouseMoved, CGPointMake(point.x, point.y), NULL);
                CGEventPost(kCGHIDEventTap, e2);
                CFRelease(e2);  
            }

            CFRelease(e1);

            return true;
        }

        bool cfilterfunc_end(piw::cfilterenv_t *env, unsigned long long to)
        {
            return false;
        }

        static inline int sign(float value)
        {
            return (value > 0) - (value < 0);
        }

        const sysin_events::sysin_events_t::impl_t *root_;

        piw::data_nb_t id_;
    };
}

piw::cfilterfunc_t * sysin_events::sysin_events_t::impl_t::cfilterctl_create(const piw::data_t &path) { return new sysin_events_func_t(this); }

sysin_events::sysin_events_t::sysin_events_t(piw::clockdomain_ctl_t *domain) : impl_(new impl_t(domain)) {}
sysin_events::sysin_events_t::~sysin_events_t() { delete impl_; }
piw::cookie_t sysin_events::sysin_events_t::cookie() { return impl_->cookie(); }
piw::change_nb_t sysin_events::sysin_events_t::press_key() { return impl_->press_key(); }
void sysin_events::sysin_events_t::set_mouse_x_scale(unsigned v) { piw::tsd_fastcall(sysin_events::sysin_events_t::impl_t::__set_mouse_x_scale,impl_,&v); }
void sysin_events::sysin_events_t::set_mouse_y_scale(unsigned v) { piw::tsd_fastcall(sysin_events::sysin_events_t::impl_t::__set_mouse_y_scale,impl_,&v); }
void sysin_events::sysin_events_t::set_mouse_x_deadband(float v) { piw::tsd_fastcall(sysin_events::sysin_events_t::impl_t::__set_mouse_x_deadband,impl_,&v); }
void sysin_events::sysin_events_t::set_mouse_y_deadband(float v) { piw::tsd_fastcall(sysin_events::sysin_events_t::impl_t::__set_mouse_y_deadband,impl_,&v); }

