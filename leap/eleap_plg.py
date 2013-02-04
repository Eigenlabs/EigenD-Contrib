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
from . import leap_version as version

import piw
import eleap_native

class Hand(atom.Atom):
    def __init__(self,agent,index):
        atom.Atom.__init__(self,names='hand',ordinal=index)

        self.__agent = agent
        self.__index = index
        
        self[1] = bundles.Output(1, False, names='palm x output')
        self[2] = bundles.Output(2, False, names='palm y output')
        self[3] = bundles.Output(3, False, names='palm z output')

        self.__output = bundles.Splitter(self.__agent.domain, self[1], self[2], self[3])
        self.__agent.eleap.create_hand(self.__index, self.__output.cookie())

class Agent(agent.Agent):
    def __init__(self, address, ordinal):
        agent.Agent.__init__(self, signature=version, names='leap', ordinal=ordinal)

        self.domain = piw.clockdomain_ctl()

        self.eleap = eleap_native.eleap(self.domain)

        self[1] = atom.Atom(names='hands')
        for i in range(1,3):
            self[1][i] = Hand(self,i)

agent.main(Agent)

