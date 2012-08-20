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
from . import audiocubes_version as version

import piw
import audiocubes_native

class AudioCube(atom.Atom):
    def __init__(self,agent,index):
        atom.Atom.__init__(self,names='audiocube',ordinal=index,protocols='remove')

        self.__agent = agent
        self.__index = index
        self[4] = bundles.Output(1, False, names='sensor output 1')
        self[5] = bundles.Output(2, False, names='sensor output 2')
        self[6] = bundles.Output(3, False, names='sensor output 3')
        self[7] = bundles.Output(4, False, names='sensor output 4')

        self.__output = bundles.Splitter(self.__agent.domain, self[4], self[5], self[6], self[7])
        self.__inputcookie = self.__agent.audiocubes.create_audiocube(self.__index, self.__output.cookie())

        self.__input = bundles.VectorInput(self.__inputcookie, self.__agent.domain, signals=(1,2,3))

        self[1] = atom.Atom(domain=domain.BoundedFloat(0,1), init=1.0, names='red input', policy=self.__input.local_policy(1,False))
        self[2] = atom.Atom(domain=domain.BoundedFloat(0,1), init=1.0, names='green input', policy=self.__input.local_policy(2,False))
        self[3] = atom.Atom(domain=domain.BoundedFloat(0,1), init=1.0, names='blue input', policy=self.__input.local_policy(3,False))

class Agent(agent.Agent):
    def __init__(self, address, ordinal):
        agent.Agent.__init__(self, signature=version, names='audiocubes', ordinal=ordinal)

        self.domain = piw.clockdomain_ctl()

        self[2] = bundles.Output(1, False, names='topology output')
        self.__output = bundles.Splitter(self.domain, self[2])

        self.audiocubes = audiocubes_native.audiocubes(self.domain, self.__output.cookie())

        self[1] = atom.Atom(names='audiocubes')
        for i in range(1,16):
            self[1][i] = AudioCube(self,i)

agent.main(Agent)

