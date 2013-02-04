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

#include <piw/piw_data.h>
#include <piw/piw_keys.h>
#include <piw/piw_thing.h>
#include <piw/piw_cfilter.h>
#include <picross/pic_stl.h>

#include "eleap.h"
 
#include "Leap_SDK/include/Leap.h"

#define HANDS 2
 
#define HAND_PALM_POSITION 0

#define FINGERS 5
 
#define DATA_PALM_POSITION 1
#define DATA_KNOWN_HANDS 2

namespace
{
    struct palm_wire_t: piw::wire_ctl_t, piw::event_data_source_real_t, virtual public pic::counted_t
    {
        palm_wire_t(unsigned);
        ~palm_wire_t();

        void start();
        void stop();
        void add_palm_position(const piw::data_nb_t &, const piw::data_nb_t &, const piw::data_nb_t &);

        piw::xevent_data_buffer_t buffer_;
        unsigned id_;
    };

    struct hand_t: piw::root_ctl_t, virtual public pic::counted_t
    {
        hand_t(eleap::eleap_t::impl_t *, unsigned, const piw::cookie_t &);
        ~hand_t();

        unsigned id() { return id_; }

        eleap::eleap_t::impl_t *root_;
        unsigned id_;
        pic::ref_t<palm_wire_t> palm_wire_;
        
        int32_t leap_id_;
    };
};

struct eleap::eleap_t::impl_t: public Leap::Listener, piw::thing_t
{
    impl_t(piw::clockdomain_ctl_t *);
    ~impl_t();
    
    void create_hand(unsigned, const piw::cookie_t &);
    void thing_dequeue_fast(const piw::data_nb_t &);

    virtual void onInit(const Leap::Controller&);
    virtual void onConnect(const Leap::Controller&);
    virtual void onDisconnect(const Leap::Controller&);
    virtual void onExit(const Leap::Controller&);
    virtual void onFrame(const Leap::Controller&);

    piw::tsd_snapshot_t ctx_;
    piw::clockdomain_ctl_t *domain_;
    Leap::Controller controller;
    pic::ref_t<hand_t> hands_[HANDS];
};


/**
 * palm_wire_t
 */

palm_wire_t::palm_wire_t(unsigned id): piw::event_data_source_real_t(piw::pathone(id,0)), id_(id)
{
    buffer_ = piw::xevent_data_buffer_t();
    buffer_.set_signal(1, piw::tsd_dataqueue(PIW_DATAQUEUE_SIZE_NORM));
    buffer_.set_signal(2, piw::tsd_dataqueue(PIW_DATAQUEUE_SIZE_NORM));
    buffer_.set_signal(3, piw::tsd_dataqueue(PIW_DATAQUEUE_SIZE_NORM));
}

palm_wire_t::~palm_wire_t()
{
    source_end(piw::tsd_time());
    piw::wire_ctl_t::disconnect();
    source_shutdown();
}

void palm_wire_t::start()
{
    unsigned long long t = piw::tsd_time();
    buffer_.add_value(1, piw::makefloat_bounded_nb(1.0,-1.0,0.0,0.0,t));
    buffer_.add_value(2, piw::makefloat_bounded_nb(1.0,-1.0,0.0,0.0,t));
    buffer_.add_value(3, piw::makefloat_bounded_nb(1.0,-1.0,0.0,0.0,t));
    source_start(0,piw::pathone_nb(id_,t),buffer_);
}

void palm_wire_t::stop()
{
    unsigned long long t = piw::tsd_time();
    buffer_.add_value(1, piw::makefloat_bounded_nb(1.0,-1.0,0.0,0.0,t));
    buffer_.add_value(2, piw::makefloat_bounded_nb(1.0,-1.0,0.0,0.0,t));
    buffer_.add_value(3, piw::makefloat_bounded_nb(1.0,-1.0,0.0,0.0,t));
    source_end(t+1);
}

void palm_wire_t::add_palm_position(const piw::data_nb_t &x, const piw::data_nb_t &y, const piw::data_nb_t &z)
{
    buffer_.add_value(1, x);
    buffer_.add_value(2, y);
    buffer_.add_value(3, z);
}


/**
 * hand_t
 */

hand_t::hand_t(eleap::eleap_t::impl_t *root, unsigned id, const piw::cookie_t &output):
    root_(root), id_(id), leap_id_(0)
{
    connect(output);

    palm_wire_t *w = new palm_wire_t(1);
    connect_wire(w, w->source());
    palm_wire_ = pic::ref(w);
}

hand_t::~hand_t()
{
}


/**
 * eleap::eleap_t::impl_t
 */

eleap::eleap_t::impl_t::impl_t(piw::clockdomain_ctl_t *domain) : domain_(domain)
{
    piw::tsd_thing(this);

    controller.addListener(*this);
}

void eleap::eleap_t::impl_t::thing_dequeue_fast(const piw::data_nb_t &d)
{
    unsigned long long dt = d.time();
    unsigned char type = dt&0xff;
    int32_t arg1 = (dt&0xffffffff)>>8;
    //unsigned char arg2 = (t&0xff0000)>>16;
    
    unsigned long long t = piw::tsd_time();
    switch(type)
    {
        case DATA_KNOWN_HANDS:
            if(d.is_array())
            {
                int32_t *hand_ids = (int32_t *)d.as_array();

                // check if known hands are still valid
                for(int i = 0; i < HANDS; ++i)
                {
                    hand_t *h = hands_[i].ptr();

                    if(h->leap_id_)
                    {
                        bool valid = false;
                        for(unsigned j = 0; j < d.as_arraylen(); ++j)
                        {
                            if(hand_ids[j] && h->leap_id_ == hand_ids[j])
                            {
                                valid = true;
                                break;
                            }
                        }
                    
                        if(!valid)
                        {
                            h->palm_wire_.ptr()->stop();
                            h->leap_id_ = 0;
                        }
                    }
                }
                
                // check if there are available hands to handle new ones
                for(unsigned i = 0; i < d.as_arraylen(); ++i)
                {
                    int32_t hand_id = hand_ids[i];
                    if(0 == hand_id)
                    {
                        continue;
                    }
                    
                    bool exists = false;
                    hand_t *available_hand = 0;
                    
                    for(int j = 0; j < HANDS; ++j)
                    {
                        hand_t *h = hands_[j].ptr();
                        if(!available_hand && 0 == h->leap_id_)
                        {
                            available_hand = h;
                        }
                        
                        if(hand_id == h->leap_id_)
                        {
                            exists = true;
                            break;
                        }
                    }
                    
                    if(!exists)
                    {
                        if(!available_hand)
                        {
                            break;
                        }
                        else
                        {
                            available_hand->leap_id_ = hand_id;
                            available_hand->palm_wire_.ptr()->start();
                        }
                    }
                }
            }
            break;
            
        case DATA_PALM_POSITION:
            if(d.is_array())
            {
                hand_t *h = 0;
                
                // try to use a known hand
                for(int i = 0; i < HANDS; ++i)
                {
                    if(hands_[i].ptr()->leap_id_ == arg1)
                    {
                        h = hands_[i].ptr();
                    }
                }
                
                if(h)
                {
                    piw::data_nb_t x = piw::makefloat_bounded_nb(1.0, -1.0, 0.0, d.as_array_member(0), t);
                    piw::data_nb_t y = piw::makefloat_bounded_nb(1.0, -1.0, 0.0, d.as_array_member(1), t);
                    piw::data_nb_t z = piw::makefloat_bounded_nb(1.0, -1.0, 0.0, d.as_array_member(2), t);
                    h->palm_wire_.ptr()->add_palm_position(x, y, z);
                }
            }
            break;
    }
}
void eleap::eleap_t::impl_t::create_hand(unsigned index, const piw::cookie_t &output)
{
    if(index < 1 || index > HANDS)
    {
        return;
    }

    hand_t *instance = new hand_t(this, index-1, output);
    hands_[index-1] = pic::ref(instance);
}

void eleap::eleap_t::impl_t::onInit(const Leap::Controller& controller)
{
}

void eleap::eleap_t::impl_t::onConnect(const Leap::Controller& controller)
{
}

void eleap::eleap_t::impl_t::onDisconnect(const Leap::Controller& controller)
{
}

void eleap::eleap_t::impl_t::onExit(const Leap::Controller& controller)
{
}

void eleap::eleap_t::impl_t::onFrame(const Leap::Controller& controller)
{
    const Leap::Frame frame = controller.frame();
    if (frame.isValid())
    {
        // send the known hands in this frame, this will handle hands coming and going
        const Leap::HandList hands = frame.hands();
        {
            unsigned long long time_encoded = (0&0xffffffff)<<8 | (DATA_KNOWN_HANDS&0xff);

            float *f;
            unsigned char *dp;
            piw::data_nb_t d = ctx_.allocate_host(time_encoded,INT32_MAX,INT32_MIN,0,BCTVTYPE_INT,sizeof(int32_t),&dp,hands.count(),&f);
            memset(f,0,hands.count()*sizeof(int32_t));
            *dp = 0;

            for(int i = 0; i < hands.count(); ++i)
            {
                const Leap::Hand hand = hands[i];
                if(hand.isValid() && hand.fingers().count() > 1)
                {
                    ((int32_t *)f)[i] = hand.id();
                }
            }
            enqueue_fast(d,1);
        }
        
        // handle the actual data for the detected hands
        for(int i = 0; i < hands.count(); ++i)
        {
            const Leap::Hand hand = hands[i];
            if(hand.isValid() && hand.fingers().count() > 1)
            {
                unsigned long long time_encoded = (hand.id()&0xffffffff)<<8 | (DATA_PALM_POSITION&0xff);

                const Leap::Vector palm_pos = hand.palmPosition();

                float *f;
                unsigned char *dp;
                piw::data_nb_t d = ctx_.allocate_host(time_encoded,600,-600,0,BCTVTYPE_FLOAT,sizeof(float),&dp,3,&f);
                memset(f,0,3*sizeof(float));
                *dp = 0;

                f[0] = piw::normalise(600,-600,0,palm_pos.x);
                f[1] = piw::normalise(600,-600,0,palm_pos.y);
                f[2] = piw::normalise(600,-600,0,palm_pos.z);

                enqueue_fast(d,1);
            }
        }
    }
}

eleap::eleap_t::impl_t::~impl_t()
{
    controller.removeListener(*this);
}


/**
 * eleap::eleap_t
 */

eleap::eleap_t::eleap_t(piw::clockdomain_ctl_t *domain) : impl_(new impl_t(domain))
{
}

eleap::eleap_t::~eleap_t()
{
    delete impl_;
}

void eleap::eleap_t::create_hand(unsigned index, const piw::cookie_t &output)
{
    impl_->create_hand(index, output);
}
