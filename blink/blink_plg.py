#
# Copyright 2012 Eigenlabs Ltd.  http://www.eigenlabs.com
#
# This file is part of EigenD.
#
# EigenD is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# EigenD is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with EigenD.  If not, see <http://www.gnu.org/licenses/>.
#

from pi import agent,atom,domain,policy,bundles,action,collection,async,utils,errors
from . import blink_manager_version as version

import piw
import blink_native

class Blink(atom.Atom):
    def __init__(self,agent,index):
        atom.Atom.__init__(self, container=(None,'blink%d'%index,agent.verb_container()), names='blink', ordinal=index)

        self.__agent = agent
        self.__index = index

        self.__inputcookie = self.__agent.blink.create_blink(self.__index)

        self.__input = bundles.VectorInput(self.__inputcookie, self.__agent.domain, signals=(1,2,3))

        self[1] = atom.Atom(domain=domain.BoundedFloat(0,1), init=1.0, names='red', policy=self.__input.local_policy(1,False))
        self[2] = atom.Atom(domain=domain.BoundedFloat(0,1), init=1.0, names='green', policy=self.__input.local_policy(2,False))
        self[3] = atom.Atom(domain=domain.BoundedFloat(0,1), init=1.0, names='blue', policy=self.__input.local_policy(3,False))

        self.add_verb2(1,'show([],~a,role(None,[instance(~self)]),role(as,[mass([red])]),role(for,[mass([second])]))',create_action=self.__show_red)
        self.add_verb2(2,'show([],~a,role(None,[instance(~self)]),role(as,[mass([green])]),role(for,[mass([second])]))',create_action=self.__show_green)
        self.add_verb2(3,'show([],~a,role(None,[instance(~self)]),role(as,[mass([blue])]),role(for,[mass([second])]))',create_action=self.__show_blue)
        
    def __show_red(self,ctx,subj,b,v,s):
        return self.__show_colour(float(action.mass_quantity(v)),None,None,action.mass_quantity(s))
        
    def __show_green(self,ctx,subj,b,v,s):
        return self.__show_colour(None,float(action.mass_quantity(v)),None,action.mass_quantity(s))
        
    def __show_blue(self,ctx,subj,b,v,s):
        return self.__show_colour(None,None,float(action.mass_quantity(v)),action.mass_quantity(s))
        
    def __show_colour(self,red,green,blue,length):
        if red is not None and (red < 0.0 or red > 1.0):
            return errors.invalid_thing(red, 'show')
        if green is not None and (green < 0.0 or green > 1.0):
            return errors.invalid_thing(green, 'show')
        if blue is not None and (blue < 0.0 or blue > 1.0):
            return errors.invalid_thing(blue, 'show')
        if length < 0:
            return errors.invalid_thing(length, 'show')
        us = int(1000000*length)
        v = piw.tuplenull_nb(0)
        v = piw.tupleadd_nb(v, piw.makelong_nb(self.__index,0))
        if red is None:
            v = piw.tupleadd_nb(v, piw.makenull_nb(0))
        else:
            v = piw.tupleadd_nb(v, piw.makefloat_nb(red,0))
        if green is None:
            v = piw.tupleadd_nb(v, piw.makenull_nb(0))
        else:
            v = piw.tupleadd_nb(v, piw.makefloat_nb(green,0))
        if blue is None:
            v = piw.tupleadd_nb(v, piw.makenull_nb(0))
        else:
            v = piw.tupleadd_nb(v, piw.makefloat_nb(blue,0))
        v = piw.tupleadd_nb(v, piw.makelong_nb(us,0))
        return piw.trigger(self.__agent.blink.show_colour(),v),None


class Agent(agent.Agent):
    def __init__(self,address,ordinal):
        agent.Agent.__init__(self, signature=version, names='blink manager', ordinal=ordinal)

        self.domain = piw.clockdomain_ctl()

        self.blink = blink_native.blink(self.domain)

        for i in range(1,17):
            self[i] = Blink(self,i)

agent.main(Agent)

