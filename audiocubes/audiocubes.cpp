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
#include <piw/piw_thing.h>
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
        void shutdown(unsigned long long);
        void add_value(const piw::data_nb_t &);

        piw::xevent_data_buffer_t buffer_;
        unsigned id_;
    };

    struct audiocube_t: piw::root_ctl_t, virtual public pic::counted_t
    {
        audiocube_t(audiocubes::audiocubes_t::impl_t *, const piw::cookie_t &);
        ~audiocube_t();

        audiocubes::audiocubes_t::impl_t *root_;
        pic::ref_t<output_wire_t> wires_[FACES];
    };
};

struct audiocubes::audiocubes_t::impl_t: piw::thing_t
{
    impl_t(piw::clockdomain_ctl_t *);
    ~impl_t();
    void thing_dequeue_fast(const piw::data_nb_t &);
    void create_audiocube(unsigned, const piw::cookie_t &);
    void set_color(unsigned, unsigned, unsigned, unsigned);

    piw::tsd_snapshot_t ctx_;
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
#if AUDIOCUBES_DEBUG>0
                pic::logmsg() << "Cubes topology change";
#endif
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
    shutdown(piw::tsd_time());
    piw::wire_ctl_t::disconnect();
    source_shutdown();
}

void output_wire_t::startup()
{
    unsigned long long t = piw::tsd_time();
    buffer_.add_value(id_, piw::makefloat_bounded_nb(1,0,0,0,t));
    source_start(0,piw::pathone_nb(id_,t),buffer_);
}

void output_wire_t::shutdown(unsigned long long t)
{
    source_end(t);
}

void output_wire_t::add_value(const piw::data_nb_t &d)
{
    buffer_.add_value(id_, d);
}


/**
 * audiocube_t
 */

audiocube_t::audiocube_t(audiocubes::audiocubes_t::impl_t *root, const piw::cookie_t &output): root_(root)
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


/**
 * audiocubes::audiocubes_t::impl_t
 */

audiocubes::audiocubes_t::impl_t::impl_t(piw::clockdomain_ctl_t *)
{
    piw::tsd_thing(this);
	CubeSetEventCallback(libraryCallback, this);
}

audiocubes::audiocubes_t::impl_t::~impl_t()
{
    CubeRemoveEventCallback(libraryCallback, this);
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
}

void audiocubes::audiocubes_t::impl_t::create_audiocube(unsigned index, const piw::cookie_t &output)
{
    if(index < 1 || index > CUBES)
    {
        return;
    }

    cubes_[index-1] = pic::ref(new audiocube_t(this, output));
}

void audiocubes::audiocubes_t::impl_t::set_color(unsigned index, unsigned red, unsigned green, unsigned blue)
{
    if(index < 1 || index > CUBES)
    {
        return;
    }

    CubeColor color;
    color.red = red;
    color.green = green;
    color.blue = blue;

    CubeSetColor(index-1, color);
}

/**
 * audiocubes::audiocubes_t
 */

audiocubes::audiocubes_t::audiocubes_t(piw::clockdomain_ctl_t *domain) : impl_(new impl_t(domain))
{
}

audiocubes::audiocubes_t::~audiocubes_t()
{
    delete impl_;
}

void audiocubes::audiocubes_t::create_audiocube(unsigned index, const piw::cookie_t &output)
{
    impl_->create_audiocube(index, output);
}

void audiocubes::audiocubes_t::set_color(unsigned index, unsigned red, unsigned green, unsigned blue)
{
    impl_->set_color(index, red, green, blue);
}
