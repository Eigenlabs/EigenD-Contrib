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

from pi import agent,atom,domain,policy,bundles
from . import vu_meter_version as version

import piw
import vu_meter_native

IN_AUDIO=11
OUT_LIGHT=56

class Agent(agent.Agent):
    def __init__(self, address, ordinal):
        agent.Agent.__init__(self, signature=version, names='vu meter', ordinal=ordinal)

        self.domain = piw.clockdomain_ctl()

        self[1] = atom.Atom(names='outputs')
        self[1][1] = bundles.Output(OUT_LIGHT, True, names='light output', protocols='revconnect')
        self.output = bundles.Splitter(self.domain, self[1][1])

        self.native = vu_meter_native.vu_meter(self.output.cookie(), self.domain)

        self.input = bundles.VectorInput(self.native.cookie(), self.domain, signals=(IN_AUDIO,))

        self[2] = atom.Atom(names='inputs')
        self[2][1] = atom.Atom(domain=domain.BoundedFloat(-1,1), names="audio input", policy=self.input.vector_policy(IN_AUDIO,policy.IsoStreamPolicy(1,-1,0)))
        
        self[3] = atom.Atom(names='thresholds')
        self[3][1] = atom.Atom(domain=domain.BoundedFloat(-90,0), init=-40.0, policy=atom.default_policy(self.__signal_level), names='signal threshold')
        self.native.set_signal_level(self[3][1].get_value())
        self[3][2] = atom.Atom(domain=domain.BoundedFloat(-90,0), init=-6.0, policy=atom.default_policy(self.__high_level), names='high threshold')
        self.native.set_high_level(self[3][2].get_value())
        self[3][3] = atom.Atom(domain=domain.BoundedFloat(-90,0), init=-1, policy=atom.default_policy(self.__clip_level), names='clip threshold')
        self.native.set_clip_level(self[3][3].get_value())
        
        self[4] = atom.Atom(domain=domain.BoundedInt(0,100), init=5, policy=atom.default_policy(self.__size), names='size')
        self.native.set_size(self[4].get_value())
        
    def __signal_level(self, value):
        self[3][1].set_value(value)
        self.native.set_signal_level(value)
        return True
    
    def __high_level(self, value):
        self[3][2].set_value(value)
        self.native.set_high_level(value)
        return True
    
    def __clip_level(self, value):
        self[3][3].set_value(value)
        self.native.set_clip_level(value)
        return True
    
    def __size(self, value):
        self[4].set_value(value)
        self.native.set_size(value)
        return True


agent.main(Agent)

