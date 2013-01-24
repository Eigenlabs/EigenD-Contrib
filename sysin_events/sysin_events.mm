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

   int press_key_char(void *, void *k_)
   {
       const char *k = *(const char **)k_;

       NSString *str = [[NSString alloc] initWithUTF8String:k];
       UniChar chr = [str characterAtIndex:0];
       CGKeyCode c = keyCodeForChar(chr);

       CGEventRef e1 = CGEventCreateKeyboardEvent(NULL, c, true);
       CGEventPost(kCGHIDEventTap, e1);
       CFRelease(e1);

       CGEventRef e2 = CGEventCreateKeyboardEvent(NULL, c, false);
       CGEventPost(kCGHIDEventTap, e2);
       CFRelease(e2);

       CFRelease(str);

       return 0;
   }

    struct mouse_input_t: piw::cfilterctl_t, piw::cfilter_t, public pic::nocopy_t
    {
        mouse_input_t(piw::clockdomain_ctl_t *domain) : cfilter_t(this, 0, domain), mouse_x_scale_(2.f), mouse_y_scale_(-1.f),
            mouse_x_deadband_(0.1f), mouse_y_deadband_(0.1f), mouse_1_down_(false), mouse_2_down_(false) {}
        
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
        
        float mouse_x_scale_;
        float mouse_y_scale_;
        float mouse_x_deadband_;
        float mouse_y_deadband_;

        bool mouse_1_down_;
        bool mouse_2_down_;
    };

    struct keypress_input_t: piw::cfilterctl_t, piw::cfilter_t, public pic::nocopy_t
    {
        keypress_input_t(sysin_events::sysin_events_t::impl_t *root, piw::clockdomain_ctl_t *domain, const unsigned index) : cfilter_t(this, 0, domain),
            root_(root), index_(index), code_(0), hold_(true) {}
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

        static int __set_hold(void *i_, void *v_)
        {
            keypress_input_t *i = (keypress_input_t *)i_;
            bool v = *(bool *)v_;
            i->hold_ = v;
            return 0;
        }

        void set_code(unsigned code)
        {
            piw::tsd_fastcall(keypress_input_t::__set_code,this,&code);
        }
        
        void set_hold(bool hold)
        {
            piw::tsd_fastcall(keypress_input_t::__set_hold,this,&hold);
        }

        sysin_events::sysin_events_t::impl_t * const root_;
        const unsigned index_;
        unsigned code_;
        bool hold_;
    };
}

namespace sysin_events
{
    struct sysin_events_t::impl_t: public pic::nocopy_t, virtual public pic::tracked_t
    {
        impl_t(piw::clockdomain_ctl_t *domain) : domain_(domain), mouseinput_(domain)
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

        piw::clockdomain_ctl_t * const domain_;
        mouse_input_t mouseinput_;
        pic::flipflop_t<std::map<unsigned,keypress_input_t *> > keypress_inputs_;
    };
}

namespace
{
    struct mouse_func_t: piw::cfilterfunc_t
    {
        mouse_func_t(mouse_input_t *root) : root_(root)
        {
        }

        bool cfilterfunc_start(piw::cfilterenv_t *env, const piw::data_nb_t &id)
        {
            id_ = id;
            
            env->cfilterenv_reset(IN_MOUSE_X, id.time());
            env->cfilterenv_reset(IN_MOUSE_Y, id.time());
            env->cfilterenv_reset(IN_MOUSE_BUTTON_1, id.time());
            env->cfilterenv_reset(IN_MOUSE_BUTTON_2, id.time());

            return true;
        }
        
        void mousevent(CGEventType type, CGPoint position, CGMouseButton button)
        {
            CGEventRef e = CGEventCreateMouseEvent(CGEventSourceCreate(kCGEventSourceStateHIDSystemState), type, position, button);
            CGEventPost(kCGHIDEventTap, e);
            CFRelease(e);  
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
                if(d.time() < from) continue;

                switch(i)
                {
                    case IN_MOUSE_X:
                        {
                            float v = d.as_norm();
                            if(fabs(v) >= root_->mouse_x_deadband_)
                            {
                                v = v - (sign(v) * root_->mouse_x_deadband_);
                                position.x += v*root_->mouse_x_scale_;
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
                                position.y += v*root_->mouse_y_scale_;
                                moved = true;
                            }
                        }
                        break;
                    case IN_MOUSE_BUTTON_1:
                        if(!root_->mouse_1_down_)
                        {
                            button1 = true;
                        }
                        break;
                    case IN_MOUSE_BUTTON_2:
                        if(!root_->mouse_2_down_)
                        {
                            button2 = true;
                        }
                        break;
                }
            }
            
            if(position.x < 0) position.x = 0;
            if(position.y < 0) position.y = 0;
            
            if(button1)
            {
                mousevent(kCGEventLeftMouseDown, position, kCGMouseButtonLeft);
                root_->mouse_1_down_ = true;
            }

            if(button2)
            {
                mousevent(kCGEventRightMouseDown, position, kCGMouseButtonRight);
                root_->mouse_2_down_ = true;
            }

            if(moved)
            {
                mousevent(kCGEventMouseMoved, position, NULL);
                
                if(root_->mouse_1_down_) mousevent(kCGEventLeftMouseDragged, position, NULL);
                if(root_->mouse_2_down_) mousevent(kCGEventRightMouseDragged, position, NULL);
            }

            return true;
        }

        bool cfilterfunc_end(piw::cfilterenv_t *env, unsigned long long to)
        {
            CGEventRef reference_event = CGEventCreate(NULL);
            CGPoint position = CGEventGetLocation(reference_event);

            if(root_->mouse_1_down_)
            {
                mousevent(kCGEventLeftMouseUp, position, kCGMouseButtonLeft);
                root_->mouse_1_down_ = false;
            }

            if(root_->mouse_2_down_)
            {
                mousevent(kCGEventRightMouseUp, position, kCGMouseButtonRight);
                root_->mouse_2_down_ = false;
            }
            
            id_ = piw::makenull_nb(to);

            return false;
        }

        static inline int sign(float value)
        {
            return (value > 0) - (value < 0);
        }

        mouse_input_t * const root_;

        piw::data_nb_t id_;
    };
    
    struct keypress_func_t: piw::cfilterfunc_t
    {
        keypress_func_t(keypress_input_t *root) : root_(root), held_(false), down_(false), down_code_(0)
        {
        }

        bool cfilterfunc_start(piw::cfilterenv_t *env, const piw::data_nb_t &id)
        {
            id_ = id;
            
            env->cfilterenv_reset(IN_KEY_PRESSURE, id.time());
            down_ = false;
            
            return true;
        }

        bool cfilterfunc_process(piw::cfilterenv_t *env, unsigned long long from, unsigned long long to, unsigned long samplerate, unsigned buffersize)
        {
            if(id_.is_null()) return false;
            
            unsigned i;
            piw::data_nb_t d;

            while(env->cfilterenv_next(i, d, to))
            {
                if(d.time() < from) continue;

                switch(i)
                {
                    case IN_KEY_PRESSURE:
                        {
                            if(!down_)
                            {
                                key_down(root_->code_);
                                down_ = true;
                                if(root_->hold_)
                                {
                                    down_code_ = root_->code_;
                                    held_ = true;
                                }
                                else
                                {
                                    key_up(root_->code_);
                                }
                            }
                        }
                        break;
                }
            }

            return true;
        }

        bool cfilterfunc_end(piw::cfilterenv_t *env, unsigned long long to)
        {
            id_ = piw::makenull_nb(to);
            
            if(held_)
            {
                key_up(down_code_);
                held_ = false;
            }
            down_ = false;
            down_code_ = 0;

            return false;
        }

        keypress_input_t * const root_;

        piw::data_nb_t id_;
        bool held_;
        bool down_;
        unsigned down_code_;
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
    root_->unregister_keypress_input(index_);
}

sysin_events::sysin_events_t::sysin_events_t(piw::clockdomain_ctl_t *domain) : impl_(new impl_t(domain)) {}
sysin_events::sysin_events_t::~sysin_events_t() { delete impl_; }
piw::cookie_t sysin_events::sysin_events_t::mouse_input() { return impl_->mouseinput_.cookie(); }
piw::cookie_t sysin_events::sysin_events_t::create_keypress_input(unsigned index) { return impl_->create_keypress_input(index); }
void sysin_events::sysin_events_t::remove_keypress_input(unsigned index) { impl_->destroy_keypress_input(index); }
piw::change_nb_t sysin_events::sysin_events_t::press_key() { return impl_->press_key(); }
void sysin_events::sysin_events_t::set_mouse_x_scale(float v) { piw::tsd_fastcall(mouse_input_t::__set_mouse_x_scale,&impl_->mouseinput_,&v); }
void sysin_events::sysin_events_t::set_mouse_y_scale(float v) { piw::tsd_fastcall(mouse_input_t::__set_mouse_y_scale,&impl_->mouseinput_,&v); }
void sysin_events::sysin_events_t::set_mouse_x_deadband(float v) { piw::tsd_fastcall(mouse_input_t::__set_mouse_x_deadband,&impl_->mouseinput_,&v); }
void sysin_events::sysin_events_t::set_mouse_y_deadband(float v) { piw::tsd_fastcall(mouse_input_t::__set_mouse_y_deadband,&impl_->mouseinput_,&v); }

void sysin_events::sysin_events_t::set_keypress_code(unsigned index, unsigned code)
{
    keypress_input_t *i = impl_->get_keypress_input(index);
    if(i)
    {
        i->set_code(code);
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

