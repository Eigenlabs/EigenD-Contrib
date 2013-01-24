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
#define IN_MASK SIG4(IN_MOUSE_X,IN_MOUSE_Y,IN_MOUSE_BUTTON_1,IN_MOUSE_BUTTON_2)

#define SYSIN_EVENTS_DEBUG 0 

namespace sysin_events
{
    struct sysin_events_t::impl_t: piw::cfilterctl_t, piw::cfilter_t, public pic::nocopy_t, virtual public pic::tracked_t
    {
        impl_t(piw::clockdomain_ctl_t *domain) : cfilter_t(this, 0, domain), mouse_x_scale_(2.f), mouse_y_scale_(-1.f),
            mouse_x_deadband_(0.2f), mouse_y_deadband_(0.2f),
            mouse_1_down_(false), mouse_2_down_(false) {}
        
        piw::cfilterfunc_t *cfilterctl_create(const piw::data_t &path);
        unsigned long long cfilterctl_thru() { return 0; }
        unsigned long long cfilterctl_inputs() { return IN_MASK; }
        unsigned long long cfilterctl_outputs() { return 0; }
        
        // The two methods below come from http://stackoverflow.com/questions/1918841/how-to-convert-ascii-character-to-cgkeycode
        
        /* Returns string representation of key, if it is printable.
         * Ownership follows the Create Rule; that is, it is the caller's
         * responsibility to release the returned object. */
        static CFStringRef createStringForKey(CGKeyCode keyCode)
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
        static CGKeyCode keyCodeForChar(UniChar character)
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
        
        static int __press_key_code(void *r_, void *k_)
        {
            unsigned k = *(unsigned *)k_;
            
            CGEventRef e1 = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)k, true);
            CGEventPost(kCGHIDEventTap, e1);
            CFRelease(e1);
            
            CGEventRef e2 = CGEventCreateKeyboardEvent(NULL, (CGKeyCode)k, false);
            CGEventPost(kCGHIDEventTap, e2);
            CFRelease(e2);

            return 0;
        }

        static int __press_key_char(void *r_, void *k_)
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
            float v = *(float *)v_;
            i->mouse_x_scale_ = v;
            return 0;
        }

        static int __set_mouse_y_scale(void *i_, void *v_)
        {
            sysin_events_t::sysin_events_t::impl_t *i = (sysin_events_t::sysin_events_t::impl_t *)i_;
            float v = *(float *)v_;
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
        
        float mouse_x_scale_;
        float mouse_y_scale_;
        float mouse_x_deadband_;
        float mouse_y_deadband_;

        bool mouse_1_down_;
        bool mouse_2_down_;
    };
}

namespace
{
    struct sysin_events_func_t: piw::cfilterfunc_t
    {
        sysin_events_func_t(sysin_events::sysin_events_t::impl_t *root) : root_(root)
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

        sysin_events::sysin_events_t::impl_t *root_;

        piw::data_nb_t id_;
    };
}

piw::cfilterfunc_t * sysin_events::sysin_events_t::impl_t::cfilterctl_create(const piw::data_t &path) { return new sysin_events_func_t(this); }

sysin_events::sysin_events_t::sysin_events_t(piw::clockdomain_ctl_t *domain) : impl_(new impl_t(domain)) {}
sysin_events::sysin_events_t::~sysin_events_t() { delete impl_; }
piw::cookie_t sysin_events::sysin_events_t::cookie() { return impl_->cookie(); }
piw::change_nb_t sysin_events::sysin_events_t::press_key() { return impl_->press_key(); }
void sysin_events::sysin_events_t::set_mouse_x_scale(float v) { piw::tsd_fastcall(sysin_events::sysin_events_t::impl_t::__set_mouse_x_scale,impl_,&v); }
void sysin_events::sysin_events_t::set_mouse_y_scale(float v) { piw::tsd_fastcall(sysin_events::sysin_events_t::impl_t::__set_mouse_y_scale,impl_,&v); }
void sysin_events::sysin_events_t::set_mouse_x_deadband(float v) { piw::tsd_fastcall(sysin_events::sysin_events_t::impl_t::__set_mouse_x_deadband,impl_,&v); }
void sysin_events::sysin_events_t::set_mouse_y_deadband(float v) { piw::tsd_fastcall(sysin_events::sysin_events_t::impl_t::__set_mouse_y_deadband,impl_,&v); }

