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

from pi import agent,atom,domain,policy,bundles,resource,utils
from . import tabulator_version as version

import piw
import datetime

DEFAULT_CHORD_TIMEOUT = 20
DEFAULT_PAGE_WIDTH = 80

class Agent(agent.Agent):
    def __init__(self, address, ordinal):
        agent.Agent.__init__(self, signature=version, names='tabulator', ordinal=ordinal)

        self.domain = piw.clockdomain_ctl()

        self.cfunctor = piw.functor_backend(1, False)
        self.cinput = bundles.VectorInput(self.cfunctor.cookie(), self.domain, signals=(1,))
        self[1] = atom.Atom(domain=domain.Aniso(), policy=self.cinput.vector_policy(1,False), names='controller input')
        self.cfunctor.set_gfunctor(utils.make_change_nb(piw.slowchange(utils.changify(self.__controller))))

        self.keyfunctor = piw.functor_backend(1, False)
        self.keyinput = bundles.VectorInput(self.keyfunctor.cookie(), self.domain, signals=(1,))
        self[2] = atom.Atom(domain=domain.Aniso(), policy=self.keyinput.vector_policy(1,False), names='key input')
        self.keyfunctor.set_gfunctor(utils.make_change_nb(piw.slowchange(utils.changify(self.__key))))

        self[3] = atom.Atom(domain=domain.BoundedInt(1,100), init=DEFAULT_CHORD_TIMEOUT, policy=atom.default_policy(self.__set_chordtimeout), names='chord timeout')
        self[4] = atom.Atom(domain=domain.BoundedInt(80,200), init=DEFAULT_PAGE_WIDTH, policy=atom.default_policy(self.__set_pagewidth), names='page width')

        self.add_verb2(1,'start([],None)',self.__start)
        self.add_verb2(2,'stop([],None)',self.__stop)

        self.__set_chordtimeout(DEFAULT_CHORD_TIMEOUT)
        self.__set_pagewidth(DEFAULT_PAGE_WIDTH)

        self.__notating = False
        self.__courselen = None
        self.__coursesqueue = None
        self.__coursesrender = None
        self.__lasttime = None

    def __set_chordtimeout(self,v):
        self[3].set_value(v)
        self.__chord_timeout = v*1000
        return True

    def __set_pagewidth(self,v):
        self[4].set_value(v)
        self.__page_width = v
        return True

    def __controller(self,c):
        if c.is_dict() and not self.__is_notating():
            self.__courselen = c.as_dict_lookup("courselen").as_tuplelen()
        return True

    def __clear_coursesqueue(self):
        self.__coursesqueue = [ list() for i in range(self.__courselen) ]

    def __clear_coursesrender(self):
        self.__coursesrender = [ "" for i in range(self.__courselen) ]

    def __print_render(self):
        for render in reversed(self.__coursesrender):
            self.__sessionhtmlfile.write(render+('-'*(self.__page_width-len(render)))+'\n')
        self.__sessionhtmlfile.write('\n')
        self.__sessionhtmlfile.flush()

    def __key(self,k):
        if self.__is_notating() and k.is_tuple() and 4 == k.as_tuplelen() and self.__courselen:

            # extract the key data elements
            physical = k.as_tuple_value(1)
            musical = k.as_tuple_value(3)

            row = int(physical.as_tuple_value(0).as_float())
            column = int(physical.as_tuple_value(1).as_float())
            course = int(musical.as_tuple_value(0).as_float())
            key = int(musical.as_tuple_value(1).as_float())

            # output the raw XML data
            self.__sessionrawfile.write('<key time="'+str(k.time())+'" ')
            self.__sessionrawfile.write('physicalseq="'+str(k.as_tuple_value(0).as_long())+'" row="'+str(row)+'" column="'+str(column)+'" ')
            self.__sessionrawfile.write('musicalseq="'+str(k.as_tuple_value(2).as_long())+'" course="'+str(course)+'" key="'+str(key)+'"/>\n')
            self.__sessionrawfile.flush()

            # render the formatted HTML tablature
            if self.__lasttime is not None and k.time()-self.__lasttime>self.__chord_timeout:

                # initialize the render of each course with a dash
                tmprender = [ "" for i in range(self.__courselen) ]

                # go over the course queues and render each key
                maxwidth = 0
                for i in range(len(self.__coursesqueue)):
                    queue = self.__coursesqueue[i]
                    queue.sort()
                    for q in queue:
                        if len(tmprender[i]) > 0:
                            tmprender[i] = tmprender[i]+'|'
                        tmprender[i] = tmprender[i]+str(q)
                    maxwidth = max(maxwidth,len(tmprender[i]))

                self.__clear_coursesqueue()

                # center each temporary render based on the maxwidth
                for i in range(len(tmprender)):
                    tmprender[i] = '-'+tmprender[i].center(maxwidth,'-')

                # increase the maxwidth by 1 for the dash prefix
                maxwidth +=1

                # check if the current staves aren't exceeding the page width
                # when the max temprender width is added
                print_render = False
                for render in self.__coursesrender:
                    if len(render)+maxwidth >= self.__page_width:
                        print_render = True
                        break

                if print_render:
                    self.__print_render()
                    self.__coursesrender = tmprender
                else:
                    for i in range(len(self.__coursesrender)):
                        self.__coursesrender[i] = self.__coursesrender[i]+tmprender[i]

            # store the key in the appropriate queue
            self.__coursesqueue[course-1].append(key)
            self.__lasttime = k.time()

        return True

    def __is_notating(self):
        return self.__notating

    def __start(self, subj=None):
        if not self.__courselen:
            return

        if self.__notating:
            self.__stop()

        current_time = datetime.datetime.now().strftime("_%Y%m%d-%H%M%S")

        self.__sessionraw = resource.new_resource_file('Tabulator','session'+current_time+'.xml',version='')
        self.__sessionrawfile = open(self.__sessionraw,'w')
        self.__sessionrawfile.write('<tabulator>\n')
        self.__sessionrawfile.flush()

        self.__sessionhtml = resource.new_resource_file('Tabulator','session'+current_time+'.html',version='')
        self.__sessionhtmlfile = open(self.__sessionhtml,'w')
        self.__sessionhtmlfile.write('<html>\n\n')
        self.__sessionhtmlfile.write('<head>\n')
        self.__sessionhtmlfile.write('<script language="javascript" type="text/javascript">\n')
        self.__sessionhtmlfile.write('<!--\n')
        self.__sessionhtmlfile.write('    var performrefresh = true\n')
        self.__sessionhtmlfile.write('    function dorefresh() {\n')
        self.__sessionhtmlfile.write('        if(performrefresh) window.location.reload()\n')
        self.__sessionhtmlfile.write('    }\n')
        self.__sessionhtmlfile.write('    setTimeout("dorefresh()",1000)\n')
        self.__sessionhtmlfile.write('//-->\n')
        self.__sessionhtmlfile.write('</script>\n')
        self.__sessionhtmlfile.write('</head>\n\n')
        self.__sessionhtmlfile.write('<body>\n')
        self.__sessionhtmlfile.write('<h1>Eigenharp Tabulator</h1>\n')
        self.__sessionhtmlfile.write('<pre>\n')
        self.__sessionhtmlfile.flush()

        self.__notating = True
        self.__clear_coursesqueue()
        self.__clear_coursesrender()

    def __stop(self, subj=None):
        if self.__notating:

            self.__print_render()

            self.__notating = False 
            self.__coursesqueue = None
            self.__lasttime = None

            self.__sessionrawfile.write('</tabulator>\n')
            self.__sessionrawfile.close()

            self.__sessionhtmlfile.write('</pre>\n')
            self.__sessionhtmlfile.write('<script language="javascript" type="text/javascript">\n')
            self.__sessionhtmlfile.write('<!--\n')
            self.__sessionhtmlfile.write('    performrefresh = false\n')
            self.__sessionhtmlfile.write('//-->\n')
            self.__sessionhtmlfile.write('</script>\n')
            self.__sessionhtmlfile.write('</body>\n')
            self.__sessionhtmlfile.write('</html>\n')
            self.__sessionhtmlfile.close()

    def close_server(self):
        agent.Agent.close_server(self)
        self.__stop()

    def on_quit(self):
        self.close_server()

agent.main(Agent)
