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

from pi import agent,atom,domain,policy,bundles,action,collection,async
from . import audiocubes_version as version

import piw
import audiocubes_native

class AudioCube(atom.Atom):
    def __init__(self,agent,index):
        atom.Atom.__init__(self,names='audiocube',ordinal=index,protocols='remove')

        self.agent = agent
        self.index = index

        self[1] = atom.Atom(domain=domain.BoundedIntOrNull(0,255,0), init=255, names='red', policy=atom.default_policy(self.__red))
        self[2] = atom.Atom(domain=domain.BoundedIntOrNull(0,255,0), init=255, names='green', policy=atom.default_policy(self.__green))
        self[3] = atom.Atom(domain=domain.BoundedIntOrNull(0,255,0), init=255, names='blue', policy=atom.default_policy(self.__blue))

        self[4] = bundles.Output(1, False, names='sensor output 1')
        self[5] = bundles.Output(2, False, names='sensor output 2')
        self[6] = bundles.Output(3, False, names='sensor output 3')
        self[7] = bundles.Output(4, False, names='sensor output 4')

        self.output = bundles.Splitter(agent.domain, self[4], self[5], self[6], self[7])
        agent.audiocubes.create_audiocube(index,self.output.cookie())

    def __update_cube_colour(self):
        self.agent.audiocubes.set_color(self.index, self[1].get_value(), self[2].get_value(), self[3].get_value())

    def __red(self,v):
        self[1].set_value(v)
        self.__update_cube_colour()

    def __green(self,v):
        self[2].set_value(v)
        self.__update_cube_colour()

    def __blue(self,v):
        self[3].set_value(v)
        self.__update_cube_colour()

class Agent(agent.Agent):
    def __init__(self, address, ordinal):
        agent.Agent.__init__(self, signature=version, names='audiocubes', ordinal=ordinal)

        self.domain = piw.clockdomain_ctl()

        self.audiocubes = audiocubes_native.audiocubes(self.domain)

        self[1] = atom.Atom(names='audiocubes')
        for i in range(1,16):
            self[1][i] = AudioCube(self,i)

agent.main(Agent)

