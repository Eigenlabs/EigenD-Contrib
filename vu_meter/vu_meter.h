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

#ifndef __vu_meter__
#define __vu_meter__

#include <piw/piw_bundle.h>
#include <piw/piw_clock.h>

namespace vu_meter
{
    class vu_meter_t
    {
        public:
            vu_meter_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain);
            ~vu_meter_t();
            piw::cookie_t cookie();

            void set_signal_level(float value);
            void set_high_level(float value);
            void set_clip_level(float value);
            void set_key_row(int value);
            void set_key_column(int value);

            class impl_t;
        private:
            impl_t *impl_;
    };
};

#endif
