#ifndef __SYNTH_NOISE__
#define __SYNTH_NOISE__

#include <piw/piw_bundle.h>
#include <piw/piw_clock.h>

namespace synth_noise
{
    class noise_t
    {
        public:
            noise_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain);
            ~noise_t();
            piw::cookie_t cookie();

            class impl_t;
        private:
            impl_t *impl_;
    };
};

#endif
