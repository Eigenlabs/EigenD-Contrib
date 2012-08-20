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

#include "audiocubes.h"
#include "libCube2/include/libCube2.h"

#define CUBES 16
#define FACES 4

#define AUDIOCUBES_DEBUG 1 
#define AUDIOCUBES_SENSOR_DEBUG 0
#define AUDIOCUBES_COLOR_DEBUG 0

namespace
{
    struct output_wire_t: piw::wire_ctl_t, piw::event_data_source_real_t, virtual public pic::counted_t
    {
        output_wire_t(unsigned);
        ~output_wire_t();

        void startup();
        void add_value(const piw::data_nb_t &);

        piw::xevent_data_buffer_t buffer_;
        unsigned id_;
    };

    struct audiocube_t: piw::root_ctl_t, piw::cfilterctl_t, piw::cfilter_t, virtual public pic::counted_t
    {
        audiocube_t(audiocubes::audiocubes_t::impl_t *, unsigned, piw::clockdomain_ctl_t *, const piw::cookie_t &);
        ~audiocube_t();

        unsigned id() { return id_; }
        void set_color(float, float, float);
        void set_color_raw(float, float, float);
        void refresh_color();

        piw::cfilterfunc_t *cfilterctl_create(const piw::data_t &);
        unsigned long long cfilterctl_thru() { return 0; }
        unsigned long long cfilterctl_inputs() { return SIG3(1,2,3); }
        unsigned long long cfilterctl_outputs() { return 0; }

        audiocubes::audiocubes_t::impl_t *root_;
        unsigned id_;
        pic::ref_t<output_wire_t> wires_[FACES];
        float last_red_;
        float last_green_;
        float last_blue_;
    };
    
    struct func_t: piw::cfilterfunc_t
    {
        func_t(audiocube_t *cube) : cube_(cube)
        {
        }

        bool cfilterfunc_start(piw::cfilterenv_t *env, const piw::data_nb_t &id)
        {
            env->cfilterenv_reset(1,id.time());
            env->cfilterenv_reset(2,id.time());
            env->cfilterenv_reset(3,id.time());
            return true;
        }

        bool cfilterfunc_process(piw::cfilterenv_t *env, unsigned long long from, unsigned long long to, unsigned long samplerate, unsigned buffersize)
        {
            bool color_changed = false;

            float red = cube_->last_red_;
            float green = cube_->last_green_;
            float blue = cube_->last_blue_;

            piw::data_nb_t d;
            if(env->cfilterenv_latest(1,d,to))
            {
                color_changed = true;
                red = d.as_renorm(0,1,0);
            }
            if(env->cfilterenv_latest(2,d,to))
            {
                color_changed = true;
                green = d.as_renorm(0,1,0);
            }
            if(env->cfilterenv_latest(3,d,to))
            {
                color_changed = true;
                blue = d.as_renorm(0,1,0);
            }

            if(color_changed)
            {
                cube_->set_color(red, green, blue);
            }

            return true;
        }

        bool cfilterfunc_end(piw::cfilterenv_t *env, unsigned long long to)
        {
            return false;
        }

        audiocube_t *cube_;
    };
    
};

struct audiocubes::audiocubes_t::impl_t: piw::root_ctl_t, piw::wire_ctl_t, piw::event_data_source_real_t, piw::thing_t
{
    impl_t(piw::clockdomain_ctl_t *, const piw::cookie_t &);
    ~impl_t();
    void thing_dequeue_fast(const piw::data_nb_t &);
    piw::cookie_t create_audiocube(unsigned, const piw::cookie_t &);
    void send_topology_entry(unsigned, unsigned, unsigned, unsigned);

    piw::tsd_snapshot_t ctx_;
    piw::clockdomain_ctl_t *domain_;
    unsigned long long time_;
    piw::xevent_data_buffer_t buffer_;
    pic::ref_t<audiocube_t> cubes_[CUBES];
};

namespace
{
    void libraryCallback(void *refCon, int numCube, unsigned int cubeEvent, unsigned int param)
    {
        audiocubes::audiocubes_t::impl_t *root = (audiocubes::audiocubes_t::impl_t *)refCon;

        switch(cubeEvent)
        {
            case CUBE_EVENT_USB_ATTACHED:
#if AUDIOCUBES_DEBUG>0
                pic::logmsg() << "Cube " << numCube+1 << " attached to USB";
#endif
                break;

            case CUBE_EVENT_USB_DETACHED:
#if AUDIOCUBES_DEBUG>0
                pic::logmsg() << "Cube " << numCube+1 << " detached from USB";
#endif
                break;

            case CUBE_EVENT_TOPOLOGY_UPDATE:
                {
#if AUDIOCUBES_DEBUG>0
                    pic::logmsg() << "Cubes topology change";
#endif
                    unsigned char topology[CUBE_MAX_TOPOLOGY_TABLE_SIZE];
                    int n = CubeTopology(topology);

                    unsigned char *dp;
                    float *vv;
                    piw::data_nb_t d = root->ctx_.allocate_host(0,1,-1,0,BCTVTYPE_BLOB,n*2,&dp,1,&vv);
                    memcpy(dp,topology,n*2);
                    *vv = n;

                    root->enqueue_fast(d,1);
                }

                break;

            case CUBE_EVENT_SENSOR_UPDATE:
                {
                    float value = CubeSensorValue(numCube, param);
#if AUDIOCUBES_SENSOR_DEBUG>0
                    pic::logmsg() << "Cube " << numCube+1 << " sensor " << param << " updated: " << value;
#endif
                    unsigned long long time_encoded = (numCube&0xf)<<4 | (param&0xf);

                    unsigned char *dp;
                    float *vv;
                    piw::data_nb_t d = root->ctx_.allocate_host(time_encoded,1,0,0,BCTVTYPE_FLOAT,sizeof(float),&dp,1,&vv);
                    *(float *)dp = value;
                    *vv = value;

                    root->enqueue_fast(d,1);
                }
                break;

            case CUBE_EVENT_COLOR_CHANGED:
#if AUDIOCUBES_COLOR_DEBUG>0
                pic::logmsg() << "Cube " << numCube+1 << " color changed";
#endif
                break;

            case CUBE_EVENT_CUBE_ADDED:
                {
#if AUDIOCUBES_DEBUG>0
                    pic::logmsg() << "Cube " << numCube+1 << " added " << param;
#endif
                    unsigned char *dp;
                    float *vv;
                    piw::data_nb_t d = root->ctx_.allocate_host(0,1000000,-1000000,0,BCTVTYPE_INT,sizeof(long),&dp,1,&vv);
                    *(long *)dp = numCube;
                    *vv = 0;

                    root->enqueue_fast(d,1);
                }
                break;
        }        
    }
}


/**
 * output_wire_t
 */

output_wire_t::output_wire_t(unsigned id): piw::event_data_source_real_t(piw::pathone(id,0)), id_(id)
{
    buffer_ = piw::xevent_data_buffer_t();
    buffer_.set_signal(id_, piw::tsd_dataqueue(PIW_DATAQUEUE_SIZE_NORM));
}

output_wire_t::~output_wire_t()
{
    source_end(piw::tsd_time());
    piw::wire_ctl_t::disconnect();
    source_shutdown();
}

void output_wire_t::startup()
{
    unsigned long long t = piw::tsd_time();
    buffer_.add_value(id_, piw::makefloat_bounded_nb(1,0,0,0,t));
    source_start(0,piw::pathone_nb(id_,t),buffer_);
}

void output_wire_t::add_value(const piw::data_nb_t &d)
{
    buffer_.add_value(id_, d);
}


/**
 * audiocube_t
 */

audiocube_t::audiocube_t(audiocubes::audiocubes_t::impl_t *root, unsigned id, piw::clockdomain_ctl_t *domain, const piw::cookie_t &output):
    cfilter_t(this, piw::cookie_t(0), domain), root_(root), id_(id),
    last_red_(-1.f), last_green_(-1.f), last_blue_(-1.f)
{
    connect(output);

    for(int i=0; i<FACES; i++)
    {
        output_wire_t *w = new output_wire_t(i+1);
        connect_wire(w, w->source());
        wires_[i] = pic::ref(w);
    }
}

audiocube_t::~audiocube_t()
{
}

piw::cfilterfunc_t *audiocube_t::cfilterctl_create(const piw::data_t &)
{
    return new func_t(this);
}

void audiocube_t::refresh_color()
{
    if(last_red_>=0 && last_green_>=0 && last_blue_>=0)
    {
        set_color_raw(last_red_, last_green_, last_blue_);
    }
}

void audiocube_t::set_color(float red, float green, float blue)
{
    if(red<0.f || red>1.f || green<0.f || green>1.f || blue<0.f || blue>1.f)
    {
        return;
    }
    if(red==last_red_ && green==last_green_ && blue==last_blue_)
    {
        return;
    }

    last_red_ = red;
    last_green_ = green;
    last_blue_ = blue;

    set_color_raw(red, green, blue);
}

void audiocube_t::set_color_raw(float red, float green, float blue)
{
    CubeColor color = {red*255,green*255,blue*255};
    CubeSetColor(id(), color);
}


/**
 * audiocubes::audiocubes_t::impl_t
 */

audiocubes::audiocubes_t::impl_t::impl_t(piw::clockdomain_ctl_t *domain, const piw::cookie_t &output) : piw::event_data_source_real_t(piw::pathone(1,0)), domain_(domain), time_(0)
{
    piw::tsd_thing(this);

    connect(output);

    buffer_ = piw::xevent_data_buffer_t();
    buffer_.set_signal(1, piw::tsd_dataqueue(PIW_DATAQUEUE_SIZE_NORM));

    connect_wire(this, source());

	CubeSetEventCallback(libraryCallback, this);
}

audiocubes::audiocubes_t::impl_t::~impl_t()
{
    CubeRemoveEventCallback(libraryCallback, this);

    piw::wire_ctl_t::disconnect();
    source_shutdown();
}

void audiocubes::audiocubes_t::impl_t::thing_dequeue_fast(const piw::data_nb_t &d)
{
    // cube addition
    if(d.is_long())
    {
        unsigned cube = d.as_long();
        if(cube>= 0 && cube<CUBES && cubes_[cube].isvalid())
        {
            for(int i=0; i<FACES; i++)
            {
                cubes_[cube].ptr()->wires_[i].ptr()->startup();
            }
            cubes_[cube].ptr()->refresh_color();
        }
    }
    // sensor update
    else if(d.is_float())
    {
        unsigned long long t = d.time();
        unsigned cube = (t&0xf0) >> 4;
        unsigned face = (t&0xf);
        if(cube>= 0 && cube<CUBES && cubes_[cube].isvalid() &&
           face>= 0 && face<FACES)
        {
            cubes_[cube].ptr()->wires_[face].ptr()->add_value(d.restamp(piw::tsd_time()));
        }
    }
    // topology update
    else if(d.is_blob())
    {
        int n = d.as_bloblen();
        if(n>0)
        {
            time_ = std::max(time_+1,piw::tsd_time());

            source_start(0,piw::pathone_nb(1,time_),buffer_);

            unsigned char *topology = (unsigned char *)d.as_blob();
            int offset = 0;
            while(n>0)
            {
                unsigned char b1 = topology[offset];
                unsigned char b2 = topology[offset+1];
                unsigned face1 = (b1&0xf)+1;
                unsigned cube1 = ((b1>>4)&0xf)+1;
                unsigned face2 = (b2&0xf)+1;
                unsigned cube2 = ((b2>>4)&0xf)+1;

                if(cube1 < cube2)
                {
                    send_topology_entry(cube1, face1, cube2, face2);
                }
                else
                {
                    send_topology_entry(cube2, face2, cube1, face1);
                }

                offset+=2;
                n-=2;
            }

            source_end(++time_);
        }
    }
}

void audiocubes::audiocubes_t::impl_t::send_topology_entry(unsigned cube1, unsigned face1, unsigned cube2, unsigned face2)
{
    piw::data_nb_t key = piw::makekey_physical(cube1*10+face1, cube2*10+face2, piw::KEY_HARD, ++time_);
    buffer_.add_value(1, key);
}

piw::cookie_t audiocubes::audiocubes_t::impl_t::create_audiocube(unsigned index, const piw::cookie_t &output)
{
    if(index < 1 || index > CUBES)
    {
        return piw::cookie_t(0);
    }

    audiocube_t *instance = new audiocube_t(this, index-1, domain_, output);
    cubes_[index-1] = pic::ref(instance);
    return instance->cookie();
}

/**
 * audiocubes::audiocubes_t
 */

audiocubes::audiocubes_t::audiocubes_t(piw::clockdomain_ctl_t *domain, const piw::cookie_t &output) : impl_(new impl_t(domain, output))
{
}

audiocubes::audiocubes_t::~audiocubes_t()
{
    delete impl_;
}

piw::cookie_t audiocubes::audiocubes_t::create_audiocube(unsigned index, const piw::cookie_t &output)
{
    return impl_->create_audiocube(index, output);
}
