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

from pi import agent,atom,domain,policy,bundles,logic
from . import fixed_lighter_version as version

import piw

class Agent(agent.Agent):
    def __init__(self, address, ordinal):
        agent.Agent.__init__(self, signature=version, names='fixed lighter', ordinal=ordinal)

        self.domain = piw.clockdomain_ctl()

        self[1] = atom.Atom(names='outputs')
        self[1][1] = bundles.Output(1, False, names='light output',protocols='revconnect')
        self.output = bundles.Splitter(self.domain, self[1][1])

        self.status_buffer = piw.statusbuffer(self.output.cookie())
        self.status_buffer.autosend(False)

        self[2] = atom.Atom(domain=domain.String(), init='[]', names='physical light map', policy=atom.default_policy(self.__physical_light_map))
        self[3] = atom.Atom(domain=domain.String(), init='[]', names='musical light map', policy=atom.default_policy(self.__musical_light_map))

    def __physical_light_map(self,v):
        self[2].set_value(v)
        self.__update_lights()

    def __musical_light_map(self,v):
        self[3].set_value(v)
        self.__update_lights()

    def __update_lights(self):
        self.status_buffer.clear()
        self.__add_lights(False,self[2].get_value())
        self.__add_lights(True,self[3].get_value())
        self.status_buffer.send()

        return True

    def __add_lights(self,musical,v):
        mapping = logic.parse_clause(v)
        for m in mapping:
            if 2 == len(m) and 2 == len(m[0]):
                colour = str(m[1]).lower()
                if colour == 'red' or colour == 'r':
                    colour = 2 
                elif colour == 'green' or colour == 'g':
                    colour = 1
                elif colour == 'orange' or colour == 'o':
                    colour =3 
                elif colour == 'off':
                    colour = 0
                colour = int(colour)
                if colour >= 0 and colour <= 3:
                    self.status_buffer.set_status(musical,int(m[0][0]),int(m[0][1]),colour)


agent.main(Agent)

