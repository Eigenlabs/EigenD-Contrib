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
#include <piw/piw_velocitydetect.h>

#include "sysin_events.h"

#include <CoreFoundation/CoreFoundation.h>
#include <Carbon/Carbon.h>

#define IN_MOUSE_X 1
#define IN_MOUSE_Y 2
#define IN_MOUSE_BUTTON_1 3
#define IN_MOUSE_BUTTON_2 4
#define IN_MOUSE_MASK SIG4(IN_MOUSE_X,IN_MOUSE_Y,IN_MOUSE_BUTTON_1,IN_MOUSE_BUTTON_2)

#define IN_KEY_PRESSURE 1
#define IN_PRESSURE_MASK SIG1(IN_KEY_PRESSURE)

#define SYSIN_EVENTS_DEBUG 0 

namespace
{
    // The two methods below come from http://stackoverflow.com/questions/1918841/how-to-convert-ascii-character-to-cgkeycode

    /* Returns string representation of key, if it is printable.
     * Ownership follows the Create Rule; that is, it is the caller's
     * responsibility to release the returned object. */
    CFStringRef createStringForKey(CGKeyCode keyCode)
    {
        TISInputSourceRef currentKeyboard = TISCopyCurrentKeyboardInputSource();
        CFDataRef layoutData = (CFDataRef)TISGetInputSourceProperty(currentKeyboard, kTISPropertyUnicodeKeyLayoutData);
        const UCKeyboardLayout *keyboardLayout =
            (const UCKeyboardLayout *)CFDataGetBytePtr(layoutData);

        UInt32 keysDown = 0;
        UniChar chars[4];
        UniCharCount realLength;

        UCKeyTranslate(keyboardLayout,
                keyCode,
                kUCKeyActionDisplay,
                0,
                LMGetKbdType(),
                kUCKeyTranslateNoDeadKeysBit,
                &keysDown,
                sizeof(chars) / sizeof(chars[0]),
                &realLength,
                chars);
        CFRelease(currentKeyboard);    

        return CFStringCreateWithCharacters(kCFAllocatorDefault, chars, 1);
    }

    /* Returns key code for given character via the above function, or UINT16_MAX
     * on error. */
    CGKeyCode keyCodeForChar(UniChar character)
    {
        static CFMutableDictionaryRef charToCodeDict = NULL;
        CGKeyCode code;
        CFStringRef charStr = NULL;

        /* Generate table of keycodes and characters. */
        if (charToCodeDict == NULL)
        {
            size_t i;
            charToCodeDict = CFDictionaryCreateMutable(kCFAllocatorDefault,
                    128,
                    &kCFCopyStringDictionaryKeyCallBacks,
                    NULL);
            if (charToCodeDict == NULL) return UINT16_MAX;

            /* Loop through every keycode (0 - 127) to find its current mapping. */
            for (i = 0; i < 128; ++i)
            {
                CFStringRef string = createStringForKey((CGKeyCode)i);
                if (string != NULL)
                {
                    CFDictionaryAddValue(charToCodeDict, string, (const void *)i);
                    CFRelease(string);
                }
            }
        }

        charStr = CFStringCreateWithCharacters(kCFAllocatorDefault, &character, 1);

        /* Our values may be NULL (0), so we need to use this function. */
        if (!CFDictionaryGetValueIfPresent(charToCodeDict, charStr, (const void **)&code))
        {
            code = UINT16_MAX;
        }

        CFRelease(charStr);
        return code;
    }

    void key_down(unsigned code)
    {
        CGEventRef e = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)code, true);
        CGEventPost(kCGHIDEventTap, e);
        CFRelease(e);
    }

    void key_up(unsigned code)
    {
        CGEventRef e = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)code, false);
        CGEventPost(kCGHIDEventTap, e);
        CFRelease(e);
    }

    int press_key_code(void *, void *k_)
    {
        unsigned k = *(unsigned *)k_;

        key_down(k);
        key_up(k);

        return 0;
    }
    
    CGKeyCode make_keycode(const char *k)
    {
        if(0 == *k)
        {
            return UINT16_MAX;
        }
        
        NSString *str = [[NSString alloc] initWithUTF8String:k];
        UniChar chr = [str characterAtIndex:0];
        CGKeyCode c = keyCodeForChar(chr);
        CFRelease(str);
        return c;
    }

    int press_key_char(void *, void *k_)
    {
        const char *k = *(const char **)k_;

        CGKeyCode c = make_keycode(k);

        CGEventRef e1 = CGEventCreateKeyboardEvent(NULL, c, true);
        CGEventPost(kCGHIDEventTap, e1);
        CFRelease(e1);

        CGEventRef e2 = CGEventCreateKeyboardEvent(NULL, c, false);
        CGEventPost(kCGHIDEventTap, e2);
        CFRelease(e2);

        return 0;
    }

    inline int sign(float value)
    {
        return (value > 0) - (value < 0);
    }

    void mousevent(CGEventType type, CGPoint position, CGMouseButton button)
    {
        CGEventRef e = CGEventCreateMouseEvent(CGEventSourceCreate(kCGEventSourceStateHIDSystemState), type, position, button);
        CGEventPost(kCGHIDEventTap, e);
        CFRelease(e);  
    }
        
    bool exceeds_threshold(float v, float threshold)
    {
        if(threshold != 0.f)
        {
            int sign_v = sign(v);
            int sign_t = sign(threshold);
            if((sign_v != sign_t) ||
               (sign_v < 0 && sign_t < 0 && v > threshold) ||
               (sign_v > 0 && sign_t > 0 && v < threshold))
            {
                return false;
            }
        }
            
        return true;
    }

    struct mouse_input_t: piw::cfilterctl_t, piw::cfilter_t, public pic::nocopy_t
    {
        mouse_input_t(sysin_events::sysin_events_t::impl_t *events, piw::clockdomain_ctl_t *domain) : cfilter_t(this, 0, domain),
            events_(events), mouse_x_scale_(2.f), mouse_y_scale_(-1.f), mouse_x_deadband_(0.1f), mouse_y_deadband_(0.1f),
            mouse_button_threshold1_(0), mouse_button_threshold2_(0), mouse_button_velocity1_(false), mouse_button_velocity2_(false),
            mouse_1_down_(false), mouse_2_down_(false) {}
        
        piw::cfilterfunc_t *cfilterctl_create(const piw::data_t &path);
        unsigned long long cfilterctl_thru() { return 0; }
        unsigned long long cfilterctl_inputs() { return IN_MOUSE_MASK; }
        unsigned long long cfilterctl_outputs() { return 0; }

        static int __set_mouse_x_scale(void *i_, void *v_)
        {
            mouse_input_t *i = (mouse_input_t *)i_;
            float v = *(float *)v_;
            i->mouse_x_scale_ = v;
            return 0;
        }

        static int __set_mouse_y_scale(void *i_, void *v_)
        {
            mouse_input_t *i = (mouse_input_t *)i_;
            float v = *(float *)v_;
            i->mouse_y_scale_ = v;
            return 0;
        }

        static int __set_mouse_x_deadband(void *i_, void *v_)
        {
            mouse_input_t *i = (mouse_input_t *)i_;
            float v = *(float *)v_;
            i->mouse_x_deadband_ = v;
            return 0;
        }

        static int __set_mouse_y_deadband(void *i_, void *v_)
        {
            mouse_input_t *i = (mouse_input_t *)i_;
            float v = *(float *)v_;
            i->mouse_y_deadband_ = v;
            return 0;
        }

        static int __set_mouse_button_threshold1(void *i_, void *v_)
        {
            mouse_input_t *i = (mouse_input_t *)i_;
            float v = *(float *)v_;
            i->mouse_button_threshold1_ = v;
            return 0;
        }

        static int __set_mouse_button_threshold2(void *i_, void *v_)
        {
            mouse_input_t *i = (mouse_input_t *)i_;
            float v = *(float *)v_;
            i->mouse_button_threshold2_ = v;
            return 0;
        }

        static int __set_mouse_button_velocity1(void *i_, void *v_)
        {
            mouse_input_t *i = (mouse_input_t *)i_;
            bool v = *(bool *)v_;
            i->mouse_button_velocity1_ = v;
            return 0;
        }

        static int __set_mouse_button_velocity2(void *i_, void *v_)
        {
            mouse_input_t *i = (mouse_input_t *)i_;
            bool v = *(bool *)v_;
            i->mouse_button_velocity2_ = v;
            return 0;
        }

        sysin_events::sysin_events_t::impl_t * const events_;
        
        float mouse_x_scale_;
        float mouse_y_scale_;
        float mouse_x_deadband_;
        float mouse_y_deadband_;
        
        float mouse_button_threshold1_;
        float mouse_button_threshold2_;
        bool mouse_button_velocity1_;
        bool mouse_button_velocity2_;

        bool mouse_1_down_;
        bool mouse_2_down_;
    };

    struct keypress_input_t: piw::cfilterctl_t, piw::cfilter_t, public pic::nocopy_t
    {
        keypress_input_t(sysin_events::sysin_events_t::impl_t *events, piw::clockdomain_ctl_t *domain, const unsigned index) : cfilter_t(this, 0, domain),
            events_(events), index_(index), hold_(true), threshold_(0.0), velocity_(false), code_(0), char_(UINT16_MAX) {}
        ~keypress_input_t();
        
        piw::cfilterfunc_t *cfilterctl_create(const piw::data_t &path);
        unsigned long long cfilterctl_thru() { return 0; }
        unsigned long long cfilterctl_inputs() { return IN_PRESSURE_MASK; }
        unsigned long long cfilterctl_outputs() { return 0; }
        
        static int __set_code(void *i_, void *v_)
        {
            keypress_input_t *i = (keypress_input_t *)i_;
            unsigned v = *(unsigned *)v_;
            i->code_ = v;
            return 0;
        }
        
        static int __set_character(void *i_, void *v_)
        {
            keypress_input_t *i = (keypress_input_t *)i_;
            CGKeyCode v = *(CGKeyCode *)v_;
            i->char_ = v;
            return 0;
        }

        static int __set_hold(void *i_, void *v_)
        {
            keypress_input_t *i = (keypress_input_t *)i_;
            bool v = *(bool *)v_;
            i->hold_ = v;
            return 0;
        }

        static int __set_velocity(void *i_, void *v_)
        {
            keypress_input_t *i = (keypress_input_t *)i_;
            bool v = *(bool *)v_;
            i->velocity_ = v;
            return 0;
        }

        static int __set_threshold(void *i_, void *v_)
        {
            keypress_input_t *i = (keypress_input_t *)i_;
            float v = *(float *)v_;
            i->threshold_ = v;
            return 0;
        }
        
        unsigned get_code()
        {
            if(char_ != UINT16_MAX)
            {
                return char_;
            }
            
            return code_;
        }

        void set_code(unsigned code)
        {
            piw::tsd_fastcall(keypress_input_t::__set_code,this,&code);
        }

        void set_character(const char *chr)
        {
            CGKeyCode code = make_keycode(chr);
            piw::tsd_fastcall(keypress_input_t::__set_character,this,&code);
        }
        
        void set_hold(bool hold)
        {
            piw::tsd_fastcall(keypress_input_t::__set_hold,this,&hold);
        }
        
        void set_threshold(float threshold)
        {
            piw::tsd_fastcall(keypress_input_t::__set_threshold,this,&threshold);
        }
        
        void set_velocity(bool velocity)
        {
            piw::tsd_fastcall(keypress_input_t::__set_velocity,this,&velocity);
        }

        sysin_events::sysin_events_t::impl_t * const events_;
        const unsigned index_;
        bool hold_;
        float threshold_;
        bool velocity_;
        
        private:
            unsigned code_;
            CGKeyCode char_;
    };
}

namespace sysin_events
{
    struct sysin_events_t::impl_t: public pic::nocopy_t, virtual public pic::tracked_t
    {
        impl_t(piw::clockdomain_ctl_t *domain) : domain_(domain), mouseinput_(this, domain)
        {
        }
        
        ~impl_t()
        {
            invalidate();
        }

        void invalidate()
        {
            std::map<unsigned,keypress_input_t *>::iterator it;
            while((it=keypress_inputs_.alternate().begin())!=keypress_inputs_.alternate().end())
            {
                delete it->second;
            }
        }

        void press_key_data(const piw::data_nb_t &d)
        {
            if(d.is_long())
            {
                unsigned code = d.as_long();
                piw::tsd_fastcall(press_key_code,(void *)0,&code);
            }
            else if(d.is_string() && d.as_stringlen() > 0)
            {
                const char* data = d.as_string();
                piw::tsd_fastcall(press_key_char,(void *)0,&data);
            }
        }

        void move_mouse_data(const piw::data_nb_t &d)
        {
            if(d.is_tuple() && d.as_tuplelen() == 2)
            {
                unsigned x = d.as_tuple_value(0).as_long();
                unsigned y = d.as_tuple_value(1).as_long();
                
                CGPoint position = CGPointMake(x,y);
                mousevent(kCGEventMouseMoved, position, NULL);
                
                if(mouseinput_.mouse_1_down_) mousevent(kCGEventLeftMouseDragged, position, NULL);
                if(mouseinput_.mouse_2_down_) mousevent(kCGEventRightMouseDragged, position, NULL);
            }
        }

        piw::cookie_t create_keypress_input(const unsigned index)
        {
            std::map<unsigned,keypress_input_t *>::iterator it;
            if((it=keypress_inputs_.alternate().find(index))!=keypress_inputs_.alternate().end())
            {
                delete it->second;
            }

            keypress_input_t *input = new keypress_input_t(this, domain_, index);
            register_keypress_input(index, input);

            return input->cookie();
        }

        void destroy_keypress_input(const unsigned index)
        {
            std::map<unsigned,keypress_input_t *>::iterator it;
            if((it=keypress_inputs_.alternate().find(index))!=keypress_inputs_.alternate().end())
            {
                delete it->second;
            }
        }
        
        void register_keypress_input(const unsigned index, keypress_input_t *input)
        {
            keypress_inputs_.alternate().insert(std::make_pair(index, input));
            keypress_inputs_.exchange();
        }

        void unregister_keypress_input(const unsigned index)
        {
            keypress_inputs_.alternate().erase(index);
            keypress_inputs_.exchange();
        }
        
        keypress_input_t *get_keypress_input(const unsigned index)
        {
            pic::flipflop_t<std::map<unsigned,keypress_input_t *> >::guard_t g(keypress_inputs_);
            
            std::map<unsigned,keypress_input_t *>::const_iterator it;
            it = g.value().find(index);
            if(it != g.value().end())
            {
                return it->second;
            }
            
            return 0;
        }

        piw::change_nb_t press_key()
        {
            return piw::change_nb_t::method(this, &sysin_events_t::impl_t::press_key_data);
        }

        piw::change_nb_t move_mouse()
        {
            return piw::change_nb_t::method(this, &sysin_events_t::impl_t::move_mouse_data);
        }

        piw::clockdomain_ctl_t * const domain_;
        mouse_input_t mouseinput_;
        pic::flipflop_t<std::map<unsigned,keypress_input_t *> > keypress_inputs_;
        piw::velocityconfig_t velocity_config_;
    };
}

namespace
{
    struct mouse_func_t: piw::cfilterfunc_t
    {
        mouse_func_t(mouse_input_t *input) : input_(input), detector_button1_(input->events_->velocity_config_), detector_button2_(input->events_->velocity_config_)
        {
        }

        bool cfilterfunc_start(piw::cfilterenv_t *env, const piw::data_nb_t &id)
        {
            id_ = id;
            
            env->cfilterenv_reset(IN_MOUSE_X, id.time());
            env->cfilterenv_reset(IN_MOUSE_Y, id.time());
            env->cfilterenv_reset(IN_MOUSE_BUTTON_1, id.time());
            env->cfilterenv_reset(IN_MOUSE_BUTTON_2, id.time());

            detector_button1_.init();
            detector_button2_.init();

            return true;
        }
        
        bool cfilterfunc_process(piw::cfilterenv_t *env, unsigned long long from, unsigned long long to, unsigned long samplerate, unsigned buffersize)
        {
            if(id_.is_null()) return false;
            
            unsigned i;
            piw::data_nb_t d;

            CGEventRef reference_event = CGEventCreate(NULL);
            CGPoint position = CGEventGetLocation(reference_event);

            bool moved = false;
            bool button1 = false;
            bool button2 = false;
            while(env->cfilterenv_next(i, d, to))
            {
                if(d.time() < from || !d.is_array()) continue;
            
                double moved_x = 0;
                double moved_y = 0;

                const float *buffer = d.as_array();
                for(unsigned j = 0; j < d.as_arraylen(); ++j)
                {
                    double v = buffer[j];

                    switch(i)
                    {
                        case IN_MOUSE_X:
                            {
                                if(fabs(v) >= input_->mouse_x_deadband_)
                                {
                                    v = v - (sign(v) * input_->mouse_x_deadband_);
                                    moved_x += v*input_->mouse_x_scale_;
                                    moved = true;
                                }
                            }
                            break;
                        case IN_MOUSE_Y:
                            {
                                if(fabs(v) >= input_->mouse_y_deadband_)
                                {
                                    v = v - (sign(v) * input_->mouse_y_deadband_);
                                    moved_y += v*input_->mouse_y_scale_;
                                    moved = true;
                                }
                            }
                            break;
                        case IN_MOUSE_BUTTON_1:
                            {
                                if(!detector_button1_.is_started())
                                {
                                    double velocity;
                                    if(detector_button1_.detect(d, &velocity) && input_->mouse_button_velocity1_)
                                    {
                                        v = velocity;
                                    }
                                }
                            
                                if(!input_->mouse_1_down_)
                                {
                                    if(exceeds_threshold(v, input_->mouse_button_threshold1_))
                                    {
                                        button1 = true;
                                    }
                                }
                                else if(!exceeds_threshold(v, input_->mouse_button_threshold1_))
                                {
                                    release_button1();
                                }
                            }
                            break;
                        case IN_MOUSE_BUTTON_2:
                            {
                                if(!detector_button2_.is_started())
                                {
                                    double velocity;
                                    if(detector_button2_.detect(d, &velocity) && input_->mouse_button_velocity2_)
                                    {
                                        v = velocity;
                                    }
                                }

                                if(!input_->mouse_2_down_)
                                {
                                    if(exceeds_threshold(v, input_->mouse_button_threshold2_))
                                    {
                                        button2 = true;
                                    }
                                }
                                else if(!exceeds_threshold(v, input_->mouse_button_threshold2_))
                                {
                                    release_button2();
                                }
                            }
                            break;
                    }
                }
                
                if(moved)
                {
                    position.x += (moved_x/d.as_arraylen())*32;
                    position.y += (moved_y/d.as_arraylen())*32;
                }
            }
            
            if(position.x < 0) position.x = 0;
            if(position.y < 0) position.y = 0;
            
            if(button1)
            {
                mousevent(kCGEventLeftMouseDown, position, kCGMouseButtonLeft);
                input_->mouse_1_down_ = true;
            }

            if(button2)
            {
                mousevent(kCGEventRightMouseDown, position, kCGMouseButtonRight);
                input_->mouse_2_down_ = true;
            }

            if(moved)
            {
                mousevent(kCGEventMouseMoved, position, NULL);
                
                if(input_->mouse_1_down_) mousevent(kCGEventLeftMouseDragged, position, NULL);
                if(input_->mouse_2_down_) mousevent(kCGEventRightMouseDragged, position, NULL);
            }

            return true;
        }

        bool cfilterfunc_end(piw::cfilterenv_t *env, unsigned long long to)
        {
            release_button1();
            release_button2();
            
            id_ = piw::makenull_nb(to);

            return false;
        }
        
        void release_button1()
        {
            if(input_->mouse_1_down_)
            {
                CGEventRef reference_event = CGEventCreate(NULL);
                CGPoint position = CGEventGetLocation(reference_event);

                mousevent(kCGEventLeftMouseUp, position, kCGMouseButtonLeft);
                input_->mouse_1_down_ = false;
            }
        }
        
        void release_button2()
        {
            if(input_->mouse_2_down_)
            {
                CGEventRef reference_event = CGEventCreate(NULL);
                CGPoint position = CGEventGetLocation(reference_event);

                mousevent(kCGEventRightMouseUp, position, kCGMouseButtonRight);
                input_->mouse_2_down_ = false;
            }
        }

        mouse_input_t * const input_;

        piw::data_nb_t id_;
        piw::velocitydetector_t detector_button1_;
        piw::velocitydetector_t detector_button2_;
    };
    
    struct keypress_func_t: piw::cfilterfunc_t
    {
        keypress_func_t(keypress_input_t *input) : input_(input), held_(false), down_(false), down_code_(0), detector_(input->events_->velocity_config_)
        {
        }

        bool cfilterfunc_start(piw::cfilterenv_t *env, const piw::data_nb_t &id)
        {
            id_ = id;
            
            env->cfilterenv_reset(IN_KEY_PRESSURE, id.time());
            down_ = false;
            detector_.init();
            
            return true;
        }

        bool cfilterfunc_process(piw::cfilterenv_t *env, unsigned long long from, unsigned long long to, unsigned long samplerate, unsigned buffersize)
        {
            if(id_.is_null()) return false;
            
            unsigned i;
            piw::data_nb_t d;

            while(env->cfilterenv_next(i, d, to))
            {
                if(d.time() < from || !d.is_array()) continue;

                const float *buffer = d.as_array();
                for(unsigned j = 0; j < d.as_arraylen(); ++j)
                {
                    double v = buffer[j];

                    switch(i)
                    {
                        case IN_KEY_PRESSURE:
                            {
                                if(!detector_.is_started())
                                {
                                    double velocity;
                                    if(detector_.detect(d, &velocity) && input_->velocity_)
                                    {
                                        v = velocity;
                                    }
                                }

                                if(!down_)
                                {
                                    if(exceeds_threshold(v, input_->threshold_))
                                    {
                                        unsigned code = input_->get_code();
                                        key_down(code);
                                        down_ = true;
                                        if(input_->hold_)
                                        {
                                            down_code_ = code;
                                            held_ = true;
                                        }
                                        else
                                        {
                                            key_up(code);
                                        }
                                    }
                                }
                                else
                                {
                                    if(!exceeds_threshold(v, input_->threshold_))
                                    {
                                        release_key();
                                    }
                                }
                            }
                            break;
                    }
                }
            }

            return true;
        }

        bool cfilterfunc_end(piw::cfilterenv_t *env, unsigned long long to)
        {
            id_ = piw::makenull_nb(to);
            
            release_key();

            return false;
        }
        
        void release_key()
        {
            if(held_)
            {
                key_up(down_code_);
                held_ = false;
            }
            down_ = false;
            down_code_ = 0;
        }

        keypress_input_t * const input_;

        piw::data_nb_t id_;
        bool held_;
        bool down_;
        unsigned down_code_;
        piw::velocitydetector_t detector_;
    };
}

piw::cfilterfunc_t *mouse_input_t::cfilterctl_create(const piw::data_t &path)
{
    return new mouse_func_t(this);
}
    
piw::cfilterfunc_t *keypress_input_t::cfilterctl_create(const piw::data_t &path)
{
    return new keypress_func_t(this);
}

keypress_input_t::~keypress_input_t()
{
    events_->unregister_keypress_input(index_);
}

sysin_events::sysin_events_t::sysin_events_t(piw::clockdomain_ctl_t *domain) : impl_(new impl_t(domain))
{
}

sysin_events::sysin_events_t::~sysin_events_t()
{
    delete impl_;
}

piw::cookie_t sysin_events::sysin_events_t::mouse_input()
{
    return impl_->mouseinput_.cookie();
}

piw::cookie_t sysin_events::sysin_events_t::create_keypress_input(unsigned index)
{
    return impl_->create_keypress_input(index);
}

void sysin_events::sysin_events_t::remove_keypress_input(unsigned index)
{
    impl_->destroy_keypress_input(index);
}

piw::change_nb_t sysin_events::sysin_events_t::press_key()
{
    return impl_->press_key();
}

piw::change_nb_t sysin_events::sysin_events_t::move_mouse()
{
    return impl_->move_mouse();
}

void sysin_events::sysin_events_t::set_mouse_x_scale(float v)
{
    piw::tsd_fastcall(mouse_input_t::__set_mouse_x_scale,&impl_->mouseinput_,&v);
}

void sysin_events::sysin_events_t::set_mouse_y_scale(float v)
{
    piw::tsd_fastcall(mouse_input_t::__set_mouse_y_scale,&impl_->mouseinput_,&v);
}

void sysin_events::sysin_events_t::set_mouse_x_deadband(float v)
{
    piw::tsd_fastcall(mouse_input_t::__set_mouse_x_deadband,&impl_->mouseinput_,&v);
}

void sysin_events::sysin_events_t::set_mouse_y_deadband(float v)
{
    piw::tsd_fastcall(mouse_input_t::__set_mouse_y_deadband,&impl_->mouseinput_,&v);
}

void sysin_events::sysin_events_t::set_mouse_button_threshold1(float v)
{
    piw::tsd_fastcall(mouse_input_t::__set_mouse_button_threshold1,&impl_->mouseinput_,&v);
}

void sysin_events::sysin_events_t::set_mouse_button_threshold2(float v)
{
    piw::tsd_fastcall(mouse_input_t::__set_mouse_button_threshold2,&impl_->mouseinput_,&v);
}

void sysin_events::sysin_events_t::set_mouse_button_velocity1(bool flag)
{
    piw::tsd_fastcall(mouse_input_t::__set_mouse_button_velocity1,&impl_->mouseinput_,&flag);
}

void sysin_events::sysin_events_t::set_mouse_button_velocity2(bool flag)
{
    piw::tsd_fastcall(mouse_input_t::__set_mouse_button_velocity2,&impl_->mouseinput_,&flag);
}

void sysin_events::sysin_events_t::set_keypress_code(unsigned index, unsigned code)
{
    keypress_input_t *i = impl_->get_keypress_input(index);
    if(i)
    {
        i->set_code(code);
    }
}

void sysin_events::sysin_events_t::set_keypress_character(unsigned index, const char *chr)
{
    keypress_input_t *i = impl_->get_keypress_input(index);
    if(i)
    {
        i->set_character(chr);
    }
}

void sysin_events::sysin_events_t::set_keypress_hold(unsigned index, bool flag)
{
    keypress_input_t *i = impl_->get_keypress_input(index);
    if(i)
    {
        i->set_hold(flag);
    }
}

void sysin_events::sysin_events_t::set_keypress_threshold(unsigned index, float threshold)
{
    keypress_input_t *i = impl_->get_keypress_input(index);
    if(i)
    {
        i->set_threshold(threshold);
    }
}

void sysin_events::sysin_events_t::set_keypress_velocity(unsigned index, bool flag)
{
    keypress_input_t *i = impl_->get_keypress_input(index);
    if(i)
    {
        i->set_velocity(flag);
    }
}

void sysin_events::sysin_events_t::set_velocity_samples(unsigned n)
{
    impl_->velocity_config_.set_samples(n);
}

void sysin_events::sysin_events_t::set_velocity_curve(float n)
{
    impl_->velocity_config_.set_curve(n);
}

void sysin_events::sysin_events_t::set_velocity_scale(float n)
{
    impl_->velocity_config_.set_scale(n);
}
