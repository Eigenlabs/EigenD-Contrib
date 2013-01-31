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

#ifndef __BLINK__
#define __BLINK__

#include <piw/piw_bundle.h>
#include <piw/piw_clock.h>

namespace blink
{
    class blink_t
    {
        public:
            blink_t(piw::clockdomain_ctl_t *domain);
            ~blink_t();

            piw::cookie_t create_blink(unsigned index);

            piw::change_nb_t show_colour();

            class impl_t;
        private:
            impl_t *impl_;
    };
};

#endif
