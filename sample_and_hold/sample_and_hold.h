#ifndef __SAMPLE_AND_HOLD__
#define __SAMPLE_AND_HOLD__

#include <piw/piw_bundle.h>
#include <piw/piw_clock.h>

namespace sample_and_hold
{
    class sample_and_hold_t
    {
        public:
            sample_and_hold_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain);
            ~sample_and_hold_t();
            piw::cookie_t cookie();

            class impl_t;
        private:
            impl_t *impl_;
    };
};

#endif
