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

from pi import agent,atom,domain,policy,bundles,action,collection,async,utils
from . import system_input_events_version as version

import piw
import sysin_events_native

class Agent(agent.Agent):
    def __init__(self, address, ordinal):
        agent.Agent.__init__(self, signature=version, names='system input events', ordinal=ordinal)

        self.domain = piw.clockdomain_ctl()

        self.sysin_events = sysin_events_native.sysin_events(self.domain)

        self.input = bundles.VectorInput(self.sysin_events.cookie(), self.domain, signals=(1,2))

        self[1] = atom.Atom(names='inputs')
        self[1][1] = atom.Atom(domain=domain.BoundedFloat(-1,1), names="mouse x input", policy=self.input.vector_policy(1,True))
        self[1][2] = atom.Atom(domain=domain.BoundedFloat(-1,1), names="mouse y input", policy=self.input.vector_policy(2,True))

        self[4] = atom.Atom(domain=domain.BoundedInt(1,100), init=10, policy=atom.default_policy(self.__set_mouse_x_scale), names='mouse x scale')
        self[5] = atom.Atom(domain=domain.BoundedInt(1,100), init=10, policy=atom.default_policy(self.__set_mouse_y_scale), names='mouse y scale')
        self[6] = atom.Atom(domain=domain.BoundedFloat(0,1), init=0.3, policy=atom.default_policy(self.__set_mouse_x_deadband), names='mouse x deadband')
        self[7] = atom.Atom(domain=domain.BoundedFloat(0,1), init=0.4, policy=atom.default_policy(self.__set_mouse_y_deadband), names='mouse y deadband')

        self.add_verb2(1,'press([],~a,role(None,[matches([key])]),role(as,[numeric]))',create_action=self.__press_key)
        self.add_verb2(2,'press([],~a,role(None,[matches([character])]),role(as,[abstract]))',create_action=self.__press_character)

    def __set_mouse_x_scale(self,v):
        self[4].set_value(v)
        self.sysin_events.set_mouse_x_scale(v)
        return True

    def __set_mouse_y_scale(self,v):
        self[5].set_value(v)
        self.sysin_events.set_mouse_y_scale(v)
        return True

    def __set_mouse_x_deadband(self,v):
        self[6].set_value(v)
        self.sysin_events.set_mouse_x_deadband(v)
        return True

    def __set_mouse_y_deadband(self,v):
        self[7].set_value(v)
        self.sysin_events.set_mouse_y_deadband(v)
        return True

    def __press_key(self,ctx,subj,dummy,val):
        v = action.abstract_wordlist(val)[0]
        v_val = int(v)
        if v_val < 0:
            return errors.invalid_thing(to, 'press')
        return piw.trigger(self.sysin_events.press_key(),piw.makelong_nb(v_val,0)),None

    def __press_character(self,ctx,subj,dummy,val):
        v = action.abstract_string(val)
        if v.startswith('!'): v=v[1:]
        return piw.trigger(self.sysin_events.press_key(),piw.makestring_nb(v,0)),None

agent.main(Agent)
