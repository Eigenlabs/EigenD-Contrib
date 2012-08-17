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

from pi import agent,atom,domain,policy,bundles,action
from . import audiocubes_version as version

import piw
import audiocubes_native

class Agent(agent.Agent):
    def __init__(self, address, ordinal):
        agent.Agent.__init__(self, signature=version, names='audiocubes', ordinal=ordinal)

        self.domain = piw.clockdomain_ctl()

        self[1] = bundles.Output(1, False, names='sensor output 1')
        self[2] = bundles.Output(2, False, names='sensor output 2')
        self[3] = bundles.Output(3, False, names='sensor output 3')
        self[4] = bundles.Output(4, False, names='sensor output 4')
        self.output = bundles.Splitter(self.domain, self[1], self[2], self[3], self[4])

        self.audiocubes = audiocubes_native.audiocubes(self.output.cookie(), self.domain)

agent.main(Agent)

