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
from . import control_voltage_calibration_version as version

import piw
import cv_calibration_native

class Agent(agent.Agent):
    def __init__(self, address, ordinal):
        agent.Agent.__init__(self, signature=version, names='control voltage calibration', ordinal=ordinal)

        self.domain = piw.clockdomain_ctl()

        self[1] = atom.Atom(names='outputs')
        self[1][1] = bundles.Output(1, False, names='voltage output')
        self[1][2] = bundles.Output(2, False, names='calibration output')
        self.output = bundles.Splitter(self.domain, self[1][1], self[1][2])

        self.cv_calibration = cv_calibration_native.cv_calibration(self.output.cookie(), self.domain)

        self.input = bundles.VectorInput(self.cv_calibration.cookie(), self.domain, signals=(1,))

        self[2] = atom.Atom(names='inputs')
        self[2][1] = atom.Atom(domain=domain.BoundedFloat(0,96000,rest=0),names="frequency input",policy=self.input.vector_policy(1,False))

        self.add_verb2(1,'calibrate([],None)',callback=self.__calibrate)

    def __calibrate(self,*arg):
        self.cv_calibration.calibrate()
        return action.nosync_return()


agent.main(Agent)

