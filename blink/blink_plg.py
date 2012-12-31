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
from . import blink_version as version

import piw
import blink_native

class Blink(atom.Atom):
    def __init__(self,agent,index):
        atom.Atom.__init__(self,names='blink',ordinal=index)

        self.__agent = agent
        self.__index = index

        self.__inputcookie = self.__agent.blink.create_blink(self.__index)

        self.__input = bundles.VectorInput(self.__inputcookie, self.__agent.domain, signals=(1,2,3))

        self[1] = atom.Atom(domain=domain.BoundedFloat(0,1), init=1.0, names='red input', policy=self.__input.local_policy(1,False))
        self[2] = atom.Atom(domain=domain.BoundedFloat(0,1), init=1.0, names='green input', policy=self.__input.local_policy(2,False))
        self[3] = atom.Atom(domain=domain.BoundedFloat(0,1), init=1.0, names='blue input', policy=self.__input.local_policy(3,False))

class Agent(agent.Agent):
    def __init__(self, address, ordinal):
        agent.Agent.__init__(self, signature=version, names='blink', ordinal=ordinal)

        self.domain = piw.clockdomain_ctl()

        self.blink = blink_native.blink(self.domain)

        self[1] = atom.Atom(names='blink')
        for i in range(1,17):
            self[1][i] = Blink(self,i)

agent.main(Agent)

