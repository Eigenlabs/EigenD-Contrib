
#
# Copyright 2009 Eigenlabs Ltd.  http://www.eigenlabs.com
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

from pi import agent,atom,domain,bundles,policy,upgrade
import piw
from . import sample_and_hold_version as version, sample_and_hold

class Agent(agent.Agent):

    def __init__(self,address, ordinal):
        agent.Agent.__init__(self,signature=version,names='sample and hold',ordinal=ordinal)

        self.domain = piw.clockdomain_ctl()

        self[1] = atom.Atom(names='outputs')
        self[1][1] = bundles.Output(1, True, names='output')
        self.output = bundles.Splitter(self.domain, self[1][1])

        self.sample_and_hold = sample_and_hold.sample_and_hold(self.output.cookie(),self.domain)

        self.input = bundles.VectorInput(self.sample_and_hold.cookie(), self.domain, signals=(1, 2))

        self[2] = atom.Atom(names='inputs')
        self[2][1] = atom.Atom(domain=domain.BoundedFloat(-1,1), names="input", policy=self.input.local_policy(1,policy.IsoStreamPolicy(1,0,0)))
        self[2][2] = atom.Atom(domain=domain.BoundedFloat(-1,1), names="gate input", policy=self.input.merge_policy(2,policy.IsoStreamPolicy(1,0,0)))
        #self[2][3] = atom.Atom(domain=domain.BoundedFloat(-1,1), names="maximum", policy=self.input.merge_policy(2,policy.DefaultPolicy(False)))
        #self[2][4] = atom.Atom(domain=domain.BoundedFloat(-1,1), names="minimum", policy=self.input.merge_policy(3,policy.DefaultPolicy(False)))

agent.main(Agent)
