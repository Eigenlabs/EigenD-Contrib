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
from . import frequency_detector_version as version

import piw
import frequency_detector_native

class Agent(agent.Agent):
    def __init__(self, address, ordinal):
        agent.Agent.__init__(self, signature=version, names='frequency detector', ordinal=ordinal)

        self.domain = piw.clockdomain_ctl()

        self[1] = atom.Atom(names='outputs')
        self[1][1] = bundles.Output(1, False, names='frequency output')
        self.output = bundles.Splitter(self.domain, self[1][1])

        self.frequency_detector = frequency_detector_native.frequency_detector(self.output.cookie(), self.domain)

        self.input = bundles.VectorInput(self.frequency_detector.cookie(), self.domain, signals=(1, 2))

        self[2] = atom.Atom(names='inputs')
        self[2][1] = atom.Atom(domain=domain.BoundedFloat(0,1), names="audio input", policy=self.input.vector_policy(1,True))

        self[3] = atom.Atom(domain=domain.BoundedFloat(0,1), init=0.01, policy=atom.default_policy(self.__threshold), names='threshold')
        self[4] = atom.Atom(domain=domain.BoundedInt(5,20), init=10, policy=atom.default_policy(self.__buffer_count), names='buffer count')

    def __threshold(self,v):
        self[3].set_value(v)
        self.frequency_detector.set_threshold(v)
        return True

    def __buffer_count(self,v):
        self[4].set_value(v)
        self.native.set_buffer_count(v)
        return True


agent.main(Agent)

