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

#ifndef __SYSIN_EVENTS__
#define __SYSIN_EVENTS__

#include <piw/piw_bundle.h>
#include <piw/piw_clock.h>

namespace sysin_events
{
    class sysin_events_t
    {
        public:
            sysin_events_t(piw::clockdomain_ctl_t *domain);
            ~sysin_events_t();
            
            piw::cookie_t mouse_input();
            piw::cookie_t create_keypress_input(unsigned index);
            void remove_keypress_input(unsigned index);

            piw::change_nb_t press_key();
  
            void set_mouse_x_scale(float v);
            void set_mouse_y_scale(float v);
            void set_mouse_x_deadband(float v);
            void set_mouse_y_deadband(float v);
            
            void set_keypress_code(unsigned index, unsigned code);
            void set_keypress_character(unsigned index, const char *chr);
            void set_keypress_hold(unsigned index, bool flag);
            void set_keypress_threshold(unsigned index, float threshold);
  
            class impl_t;
        private:
            impl_t *impl_;
    };
};

#endif
