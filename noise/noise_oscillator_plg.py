
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

from pi import agent, atom, domain, bundles, policy, upgrade
import piw
from . import noise_oscillator_version as version, synth_noise

class Agent(agent.Agent):

    def __init__(self,address, ordinal):
        agent.Agent.__init__(self, signature=version, names='noise oscillator',
            ordinal=ordinal)

        self.domain = piw.clockdomain_ctl()

        # One Iso output for the noise
        self[1] = atom.Atom(names='outputs')
        self[1][1] = bundles.Output(1, True, names='audio output')
        self.output = bundles.Splitter(self.domain, self[1][1])

        self.osc = synth_noise.noise(self.output.cookie(),self.domain)

        # Two Iso inputs, volume, and filter freq
        self.input = bundles.VectorInput(self.osc.cookie(), self.domain, 
            signals=(1,2))
        self[2] = atom.Atom(names='inputs')
        self[2][1] = atom.Atom(domain=domain.BoundedFloat(0,1), 
            names="volume input", 
            policy=self.input.local_policy(1, policy.IsoStreamPolicy(1, 0, 0)))
        self[2][2] = atom.Atom(domain=domain.BoundedFloat(0, 20000), 
            names="filter frequency input", 
            policy=self.input.merge_policy(2, True))


agent.main(Agent)
