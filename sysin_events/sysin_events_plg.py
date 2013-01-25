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
from . import system_input_events_version as version

import piw
import sysin_events_native

class KeyPress(atom.Atom):
    def __init__(self,agent,index):
        atom.Atom.__init__(self,names='key press',ordinal=index,protocols='remove')

        self.__agent = agent
        self.__index = index

        self.__inputcookie = self.__agent.sysin_events.create_keypress_input(self.__index)
        self.__input = bundles.VectorInput(self.__inputcookie, agent.domain, signals=(1,))

        self[1] = atom.Atom(domain=domain.BoundedFloat(-1,1), names="pressure input", policy=self.__input.vector_policy(1,False))
        
        self[2] = atom.Atom(domain=domain.BoundedInt(0,127), names='key code', policy=atom.default_policy(self.__set_code))
        self[3] = atom.Atom(domain=domain.String(), names='character', init='', policy=atom.default_policy(self.__set_character))
        self[4] = atom.Atom(domain=domain.Bool(), names='hold', init=True, policy=atom.default_policy(self.__set_hold))
        self[5] = atom.Atom(domain=domain.BoundedFloat(-1,1), names='threshold', init=0.0, policy=atom.default_policy(self.__set_threshold))
        self[6] = atom.Atom(domain=domain.Bool(), names='velocity', init=False, policy=atom.default_policy(self.__set_velocity))

    def __set_code(self,v):
        self[2].set_value(v)
        self.__agent.sysin_events.set_keypress_code(self.__index, v)
        return True

    def __set_character(self,v):
        self[3].set_value(v)
        self.__agent.sysin_events.set_keypress_character(self.__index, v)
        return True

    def __set_hold(self,v):
        self[4].set_value(v)
        self.__agent.sysin_events.set_keypress_hold(self.__index, v);
        return True

    def __set_threshold(self,v):
        self[5].set_value(v)
        self.__agent.sysin_events.set_keypress_threshold(self.__index, v)
        return True

    def __set_velocity(self,v):
        self[6].set_value(v)
        self.__agent.sysin_events.set_keypress_velocity(self.__index, v)
        return True

    def disconnect(self):
        self.__agent.sysin_events.remove_keypress_input(self.__index)

class KeyPresses(collection.Collection):
    def __init__(self,agent):
        self.__agent = agent
        self.__timestamp = piw.tsd_time()

        collection.Collection.__init__(self,names="key press inputs",creator=self.__create_keypress,wrecker=self.__wreck_keypress,inst_creator=self.__create_inst,inst_wrecker=self.__wreck_inst)
        self.update()

    def update(self):
        self.__timestamp = self.__timestamp+1
        self.set_property_string('timestamp',str(self.__timestamp))

    def new_keypress(self,index):
        return KeyPress(self.__agent, index)
    
    def keypresses_changed(self):
        pass
    
    def __create_keypress(self,index):
        return self.new_keypress(index)
    
    def __wreck_keypress(self,index,node):
        node.disconnect()
        self.keypresses_changed()

    def create_keypress(self,ordinal=None):
        o = ordinal or self.find_hole()
        o = int(o)
        e = self.new_keypress(o)
        self[o] = e
        e.set_ordinal(int(o))
        self.keypresses_changed()
        self.__agent.update()
        return e 

    def get_keypress(self,index):
        return self.get(index)

    def uncreate(self,oid):
        for k,v in self.items():
            if v.id()==oid:
                self.del_keypress(k)
                return True
        return False

    def del_keypress(self,index):
        v = self[index]
        del self[index]
        v.disconnect()
        self.keypresses_changed()
        self.__agent.update()
    
    @async.coroutine('internal error')
    def __create_inst(self,ordinal=None):
        e = self.create_keypress(ordinal)
        yield async.Coroutine.success(e)

    @async.coroutine('internal error')
    def __wreck_inst(self,key,inst,ordinal):
        inst.disconnect()
        self.keypresses_changed()
        yield async.Coroutine.success()
        
class Agent(agent.Agent):
    def __init__(self, address, ordinal):
        agent.Agent.__init__(self, signature=version, names='system input events', ordinal=ordinal)

        self.domain = piw.clockdomain_ctl()

        self.sysin_events = sysin_events_native.sysin_events(self.domain)

        self.__input = bundles.VectorInput(self.sysin_events.mouse_input(), self.domain, signals=(1,2,3,4))

        self[1] = atom.Atom(names='mouse inputs')
        self[1][1] = atom.Atom(domain=domain.BoundedFloat(-1,1), names="mouse horizontal input", policy=self.__input.vector_policy(1,False))
        self[1][2] = atom.Atom(domain=domain.BoundedFloat(-1,1), names="mouse vertical input", policy=self.__input.vector_policy(2,False))
        self[1][3] = atom.Atom(domain=domain.BoundedFloat(-1,1), names="left mouse button input", policy=self.__input.vector_policy(3,False))
        self[1][4] = atom.Atom(domain=domain.BoundedFloat(-1,1), names="right mouse button input", policy=self.__input.vector_policy(4,False))

        self[2] = KeyPresses(self)
        
        self[3] = atom.Atom(names='velocity curve controls')
        self[3][1] = atom.Atom(domain=domain.BoundedInt(1,1000), init=4, names="velocity sample", policy=atom.default_policy(self.__set_samples))
        self[3][2] = atom.Atom(domain=domain.BoundedFloat(0.1,10), init=4, names="velocity curve", policy=atom.default_policy(self.__set_curve))
        self[3][3] = atom.Atom(domain=domain.BoundedFloat(0.1,10), init=4, names="velocity scale", policy=atom.default_policy(self.__set_scale))

        self[4] = atom.Atom(names='mouse controls')
        self[4][1] = atom.Atom(domain=domain.BoundedFloat(-10,10), init=2.0, policy=atom.default_policy(self.__set_mouse_x_scale), names='mouse horizontal scale')
        self[4][2] = atom.Atom(domain=domain.BoundedFloat(-10,10), init=-1.0, policy=atom.default_policy(self.__set_mouse_y_scale), names='mouse vertical scale')
        self[4][3] = atom.Atom(domain=domain.BoundedFloat(0,1), init=0.1, policy=atom.default_policy(self.__set_mouse_x_deadband), names='mouse horizontal deadband')
        self[4][4] = atom.Atom(domain=domain.BoundedFloat(0,1), init=0.1, policy=atom.default_policy(self.__set_mouse_y_deadband), names='mouse vertical deadband')
        self[4][5] = atom.Atom(domain=domain.BoundedFloat(-1,1), names='mouse button threshold 1', init=0.0, policy=atom.default_policy(self.__set_mouse_button_threshold1))
        self[4][6] = atom.Atom(domain=domain.BoundedFloat(-1,1), names='mouse button threshold 2', init=0.0, policy=atom.default_policy(self.__set_mouse_button_threshold2))
        self[4][7] = atom.Atom(domain=domain.Bool(), names='mouse button velocity 1', init=False, policy=atom.default_policy(self.__set_mouse_button_velocity1))
        self[4][8] = atom.Atom(domain=domain.Bool(), names='mouse button velocity 2', init=False, policy=atom.default_policy(self.__set_mouse_button_velocity2))

        self.add_verb2(1,'press([],~a,role(None,[matches([key])]),role(as,[numeric]))',create_action=self.__press_key)
        self.add_verb2(2,'press([],~a,role(None,[matches([character])]),role(as,[abstract]))',create_action=self.__press_character)

    def __set_samples(self,v):
        self[3][1].set_value(v)
        self.sysin_events.set_velocity_samples(v)
        return True

    def __set_curve(self,v):
        self[3][2].set_value(v)
        self.sysin_events.set_velocity_curve(v)
        return True

    def __set_scale(self,v):
        self[3][3].set_value(v)
        self.sysin_events.set_velocity_scale(v)
        return True

    def __set_mouse_x_scale(self,v):
        self[4][1].set_value(v)
        self.sysin_events.set_mouse_x_scale(v)
        return True

    def __set_mouse_y_scale(self,v):
        self[4][2].set_value(v)
        self.sysin_events.set_mouse_y_scale(v)
        return True

    def __set_mouse_x_deadband(self,v):
        self[4][3].set_value(v)
        self.sysin_events.set_mouse_x_deadband(v)
        return True

    def __set_mouse_y_deadband(self,v):
        self[4][4].set_value(v)
        self.sysin_events.set_mouse_y_deadband(v)
        return True

    def __set_mouse_button_threshold1(self,v):
        self[4][5].set_value(v)
        self.sysin_events.set_mouse_button_threshold1(v)
        return True

    def __set_mouse_button_threshold2(self,v):
        self[4][6].set_value(v)
        self.sysin_events.set_mouse_button_threshold2(v)
        return True

    def __set_mouse_button_velocity1(self,v):
        self[4][7].set_value(v)
        self.sysin_events.set_mouse_button_velocity1(v)
        return True

    def __set_mouse_button_velocity2(self,v):
        self[4][8].set_value(v)
        self.sysin_events.set_mouse_button_velocity2(v)
        return True

    def __press_key(self,ctx,subj,dummy,val):
        v = action.abstract_wordlist(val)[0]
        v_val = int(v)
        if v_val < 0:
            return errors.invalid_thing(to, 'press')
        return piw.trigger(self.sysin_events.press_key(),piw.makelong_nb(v_val,0)),None

    def __press_character(self,ctx,subj,dummy,val):
        v = action.abstract_string(val)
        if v.startswith('!'): v=v[1:]
        return piw.trigger(self.sysin_events.press_key(),piw.makestring_nb(v,0)),None

agent.main(Agent)
