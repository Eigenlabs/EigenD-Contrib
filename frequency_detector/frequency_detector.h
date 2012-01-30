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

#ifndef __FREQUENCY_DETECTOR__
#define __FREQUENCY_DETECTOR__

#include <piw/piw_bundle.h>
#include <piw/piw_clock.h>

namespace frequency_detector
{
    class frequency_detector_t
    {
        public:
            frequency_detector_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain);
            ~frequency_detector_t();
            piw::cookie_t cookie();

            void set_threshold(float v);
            void set_buffer_count(unsigned v);


            class impl_t;
        private:
            impl_t *impl_;
    };
};

#endif
