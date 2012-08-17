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
        output_wire_t(audiocubes::audiocubes_t::impl_t *i, unsigned);
        ~output_wire_t();

        void startup();
        void shutdown(unsigned long long t);
        void add_value(unsigned, const piw::data_nb_t &);

        audiocubes::audiocubes_t::impl_t *root_;
        piw::xevent_data_buffer_t buffer_;
        unsigned id_;
    };
};

struct audiocubes::audiocubes_t::impl_t: piw::root_ctl_t, piw::thing_t
{
    impl_t(const piw::cookie_t &, piw::clockdomain_ctl_t *);
    ~impl_t();
    void thing_dequeue_fast(const piw::data_nb_t &d);

    piw::tsd_snapshot_t ctx_;
    pic::ref_t<output_wire_t> wires_[CUBES];
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

output_wire_t::output_wire_t(audiocubes::audiocubes_t::impl_t *root, unsigned id): piw::event_data_source_real_t(piw::pathone(id,0)), root_(root), id_(id)
{
    buffer_ = piw::xevent_data_buffer_t();
    for(int i=1; i<=FACES; ++i)
    {
        buffer_.set_signal(i,piw::tsd_dataqueue(PIW_DATAQUEUE_SIZE_NORM));
    }
    root_->connect_wire(this,source());
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
    for(int i=1; i<=FACES; ++i)
    {
        buffer_.add_value(i,piw::makefloat_bounded_nb(1,0,0,0,t));
    }
    source_start(0,piw::pathone_nb(id_,t),buffer_);
}

void output_wire_t::shutdown(unsigned long long t)
{
    source_end(t);
}

void output_wire_t::add_value(unsigned sig, const piw::data_nb_t &d)
{
    buffer_.add_value(sig,d);
}


/**
 * audiocubes::audiocubes_t::impl_t
 */

audiocubes::audiocubes_t::impl_t::impl_t(const piw::cookie_t &c, piw::clockdomain_ctl_t *d)
{
    piw::tsd_thing(this);
    connect(c);

    for(int i=0; i<CUBES; i++)
    {
        wires_[i] = pic::ref(new output_wire_t(this, i));
    }
	CubeSetEventCallback(libraryCallback, this);
}

void audiocubes::audiocubes_t::impl_t::thing_dequeue_fast(const piw::data_nb_t &d)
{
    // cube addition
    if(d.is_long())
    {
        unsigned cube = d.as_long();
        if(cube>= 0 && cube<CUBES)
        {
            wires_[cube].ptr()->startup();
        }
    }
    // sensor update
    else if(d.is_float())
    {
        unsigned long long t = d.time();
        unsigned cube = (t&0xf0) >> 4;
        unsigned face = (t&0xf);
        if(cube<CUBES)
        {
            wires_[cube].ptr()->add_value(face+1, d.restamp(piw::tsd_time()));
        }
    }
}

audiocubes::audiocubes_t::impl_t::~impl_t()
{
    CubeRemoveEventCallback(libraryCallback, this);
}


/**
 * audiocubes::audiocubes_t
 */

audiocubes::audiocubes_t::audiocubes_t(const piw::cookie_t &output, piw::clockdomain_ctl_t *domain) : impl_(new impl_t(output, domain))
{
}

audiocubes::audiocubes_t::~audiocubes_t()
{
    delete impl_;
}
