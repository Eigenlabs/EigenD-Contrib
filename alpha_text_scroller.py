#!/usr/bin/python
# coding=UTF-8

#
# This script uses the Illuminator agent's HTTP interface to display
# scrolling text on the Eigenharp Alpha.
#
# Copyright 2012 Eigenlabs Ltd.   http://www.eigenlabs.com
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import httplib
import time
import argparse

chars = {
' ': (' ', \
      ' ', \
      ' ', \
      ' ', \
      ' '),
     
'!': (' 0 ', \
      ' 0 ', \
      ' 0 ', \
      '   ', \
      ' 0 '),
     
'"': ('00', \
      '00', \
      '  ', \
      '  ', \
      '  '),
     
'#': (' 0 0 ', \
      '00000', \
      ' 0 0 ', \
      '00000', \
      ' 0 0 '),
     
'$': (' 000 ', \
      '0 0  ', \
      ' 000 ', \
      '  0 0', \
      ' 000 '),
     
'%': (' 0   ', \
      '   0 ', \
      '  0  ', \
      ' 0   ', \
      '   0 '),
     
'&': (' 00  ', \
      '0  0 ', \
      ' 00  ', \
      '0  00', \
      ' 00 0'),
     
'\'': (' 0 ', \
      ' 0 ', \
      '   ', \
      '   ', \
      '   '),
     
'(': (' 0', \
      '0 ', \
      '0 ', \
      '0 ', \
      ' 0'),
     
')': ('0 ', \
      ' 0', \
      ' 0', \
      ' 0', \
      '0 '),
     
'*': ('0 0 0', \
      ' 000 ', \
      '00000', \
      ' 000 ', \
      '0 0 0'),
     
'+': ('   ', \
      ' 0 ', \
      '000', \
      ' 0 ', \
      '   '),
     
',': ('  ', \
      '  ', \
      ' 0', \
      ' 0', \
      '0 '),
     
'-': ('   ', \
      '   ', \
      '000', \
      '   ', \
      '   '),
     
'/': ('   ', \
      '  0', \
      ' 0 ', \
      '0  ', \
      '   '),
     
'.': (' ', \
      ' ', \
      ' ', \
      ' ', \
      '0'),
     
':': (' ', \
      '0', \
      ' ', \
      '0', \
      ' '),
     
';': ('  ', \
      ' 0', \
      '  ', \
      ' 0', \
      '0 '),
     
'<': ('  0', \
      ' 0 ', \
      '0  ', \
      ' 0 ', \
      '  0'),
     
'=': ('   ', \
      '000', \
      '   ', \
      '000', \
      '   '),
     
'>': ('0  ', \
      ' 0 ', \
      '  0', \
      ' 0 ', \
      '0  '),
     
'?': ('00 ', \
      '  0', \
      ' 0 ', \
      '   ', \
      ' 0 '),
     
'@': (' 000 ', \
      '0 0 0', \
      '0  0 ', \
      '0    ', \
      ' 000 '),
     
'[': ('00', \
      '0 ', \
      '0 ', \
      '0 ', \
      '00'),
     
'\\': ('   ', \
      '0  ', \
      ' 0 ', \
      '  0', \
      '   '),
     
']': ('00', \
      ' 0', \
      ' 0', \
      ' 0', \
      '00'),
     
'^': (' 0 ', \
      '0 0', \
      '   ', \
      '   ', \
      '   '),
     
'_': ('   ', \
      '   ', \
      '   ', \
      '   ', \
      '000'),
     
'`': ('0 ', \
      ' 0', \
      '  ', \
      '  ', \
      '  '),
     
'{': (' 00', \
      ' 0 ', \
      '0  ', \
      ' 0 ', \
      ' 00'),
     
'|': ('0', \
      '0', \
      '0', \
      '0', \
      '0'),
     
'}': ('00 ', \
      ' 0 ', \
      '  0', \
      ' 0 ', \
      '00 '),
     
'~': ('       ', \
      ' 00    ', \
      '0  0  0', \
      '    00 ', \
      '       '),
     
'€': ('  000', \
      '000  ', \
      ' 0   ', \
      '000  ', \
      '  000'),
     
'£': ('  00', \
      ' 0  ', \
      '0000', \
      ' 0  ', \
      '0000'),
     
'A': (' 00 ', \
      '0  0', \
      '0000', \
      '0  0', \
      '0  0'),
     
'B': ('000 ', \
      '0  0', \
      '000 ', \
      '0  0', \
      '000 '),
     
'C': (' 000', \
      '0   ', \
      '0   ', \
      '0   ', \
      ' 000'),
     
'D': ('000 ', \
      '0  0', \
      '0  0', \
      '0  0', \
      '000 '),
     
'E': ('000', \
      '0  ', \
      '00 ', \
      '0  ', \
      '000'),
     
'F': ('000', \
      '0  ', \
      '00 ', \
      '0  ', \
      '0  '),
     
'G': (' 00 ', \
      '0   ', \
      '0 00', \
      '0  0', \
      ' 00 '),
     
'H': ('0  0', \
      '0  0', \
      '0000', \
      '0  0', \
      '0  0'),
     
'I': ('000', \
      ' 0 ', \
      ' 0 ', \
      ' 0 ', \
      '000'),
     
'J': ('000', \
      '  0', \
      '  0', \
      '  0', \
      '00 '),
     
'K': ('0  0', \
      '0 0 ', \
      '00  ', \
      '0 0 ', \
      '0  0'),
     
'L': ('0  ', \
      '0  ', \
      '0  ', \
      '0  ', \
      '000'),
     
'M': ('0   0', \
      '00 00', \
      '0 0 0', \
      '0   0', \
      '0   0'),
     
'N': ('0  0', \
      '00 0', \
      '0 00', \
      '0  0', \
      '0  0'),
     
'O': (' 00 ', \
      '0  0', \
      '0  0', \
      '0  0', \
      ' 00 '),
     
'P': ('000 ', \
      '0  0', \
      '000 ', \
      '0   ', \
      '0   '),
     
'Q': (' 00  ', \
      '0  0 ', \
      '0 00 ', \
      '0  0 ', \
      ' 00 0'),
     
'R': ('000 ', \
      '0  0', \
      '000 ', \
      '0 0 ', \
      '0  0'),
     
'S': (' 000', \
      '0   ', \
      ' 00 ', \
      '   0', \
      '000 '),
     
'T': ('000', \
      ' 0 ', \
      ' 0 ', \
      ' 0 ', \
      ' 0 '),
     
'U': ('0  0', \
      '0  0', \
      '0  0', \
      '0  0', \
      ' 00 '),
     
'V': ('0   0', \
      '0   0', \
      '0   0', \
      ' 0 0 ', \
      '  0  '),
     
'W': ('0   0', \
      '0   0', \
      '0 0 0', \
      '00 00', \
      '0   0'),
     
'X': ('0   0', \
      ' 0 0 ', \
      '  0  ', \
      ' 0 0 ', \
      '0   0'),
     
'Y': ('0   0', \
      ' 0 0 ', \
      '  0  ', \
      '  0  ', \
      '  0  '),
     
'Z': ('0000', \
      '   0', \
      '  0 ', \
      ' 0  ', \
      '0000'),
     
'0': (' 00 ', \
      '0  0', \
      '0 00', \
      '00 0', \
      ' 00 '),
     
'1': (' 0 ', \
      '00 ', \
      ' 0 ', \
      ' 0 ', \
      '000'),
     
'2': (' 00 ', \
      '0  0', \
      '  0 ', \
      ' 0  ', \
      '0000'),
     
'3': ('000 ', \
      '   0', \
      ' 00 ', \
      '   0', \
      '000 '),
     
'4': ('  0', \
      ' 00', \
      '0 0', \
      '000', \
      '  0'),
     
'5': ('000', \
      '0  ', \
      ' 0 ', \
      '  0', \
      '00 '),
     
'6': (' 00 ', \
      '0   ', \
      '000 ', \
      '0  0', \
      ' 00 '),
     
'7': ('0000', \
      '   0', \
      '  0 ', \
      ' 0  ', \
      '0   '),
     
'8': (' 00 ', \
      '0  0', \
      '0000', \
      '0  0', \
      ' 00 '),
     
'9': (' 00 ', \
      '0  0', \
      ' 000', \
      '   0', \
      '000 '),
}

class DisplayOptions:
    """Used to configure the text display."""

    __slots__ = ('host','port','colour','speed','repeat','spacing','flip','direction','type')

    def __init__(self, port, host='localhost', colour='R', speed=1, repeat=1, spacing=1, flip=False, direction='left', type='physical'):
        self.host = host
        self.port = port
        self.colour = colour
        self.speed = speed
        self.repeat = repeat
        self.spacing = spacing
        self.flip = flip
        self.direction = direction
        self.type = type

def create_bitmap(text, o):
    """Create a bitmap out of ASCII text.

    Arguments:
    text -- the text that will be converted
    o    -- an instance of DisplayOptions to configure the bitmap conversion

    Returns:
    a list with string lines that constitute the bitmap
    """
    colour = o.colour
    content = ['','','','','']
    escape = False
    escape_mode = None
    escape_var = None
    for i in text.upper():
        if escape:
            if escape_mode is None:
                escape_mode = i
                escape_var = None
                continue
            if escape_var is None:
                if i == '{':
                    escape_var = ''
                    continue
                else:
                    escape_mode = None
                    escape = False
            if escape_var is not None:
                if i == '}':
                    if escape_mode == 'C':
                        colour = escape_var[:1]
                    elif escape_mode == 'S':
                        space = int(escape_var)
                        newcontent = []
                        for line in content:
                            newcontent.append(line+' '*space)
                        content = newcontent
                    escape_mode = None
                    escape = False
                else:
                    escape_var += i
                continue
        if i == '\\':
            escape = True
            escape_mode = None
            continue
        
        # convert the text into bitmap lines
        if i in chars:
            content_char = chars[i]
            newcontent = []
            for idx,val in enumerate(content):
                newline = val
                if idx < len(content_char):
                    newline += content_char[idx].replace('0',colour)
                    newcontent.append(newline + ' '*o.spacing)
            content = newcontent

    # flip the content if needed
    #if o.flip:
    #    content.reverse()

    return content

def scroll(text, o):
    """Scroll ASCII text as a bitmap on the Eigenharp keyboard.

    Arguments:
    text -- the text that will be converted
    o    -- an instance of DisplayOptions to configure the bitmap conversion
    """
    connection = httplib.HTTPConnection(o.host+':'+str(o.port))

    content = create_bitmap(text, o)
    
    # calculate the line length
    length = 0
    for line in content:
        length = max(length,len(line))

    # calculate how many total iterations to scroll the bitmap
    remaining = length * o.repeat

    while 0 == o.repeat or remaining >= 0:
        # limit the bitmap width to 24 pixels
        requestcontent = []
        for line in content:
            if o.flip:
                requestcontent.append(line[:24])
            else:
                requestcontent.append(line[:24][::-1])

        # display the bitmap
        connection.request('PUT', '/'+o.type, '\n'.join(requestcontent))
        connection.getresponse()

        # rotate the bitmap around by one position
        newcontent = []
        for line in content:
            if len(line) > 1:
                if o.direction == 'left':
                    line = line[1:]+line[0]
                elif o.direction == 'right':
                    line = line[-1]+line[:-1]
            newcontent.append(line)
        content = newcontent

        # wait for next step
        if o.speed == 0:
            while True:
                time.sleep(100)
        else:
            time.sleep(0.1/o.speed)

        if o.repeat:
            remaining -= 1

if __name__ == "__main__":
    parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter,description="""Display and scroll ASCII text on the Eigenharp Alpha keyboard.

This program requires the use of an Illuminator agent in your Eigenharp
Alpha setup. The server port of the Illuminator has to be set to another
number than 0 to start up the built-in HTTP server. This port number is what
you have to provide to the -p argument to make this text scroller communicate
with the right Illuminator.

Escape codes can be used to format the text, these have the following syntax:

   \\x{y}

where x is a character that select the escape code
and y is an argument that's used to configure the escape code

The following escape codes are understood:

   \\c  changes the active colour, valid arguments are red, green and orange
   \\s  inserts blank space, the argument is a number that indicates the
        width of the space
""")
    parser.add_argument('text',type=str,help='the text will be converted to a bitmap')
    parser.add_argument('-H','--host',type=str,default='localhost',help='the host or IP address on which the illuminator agent is listening, defaults to localhost')
    parser.add_argument('-p','--port',type=int,required=True,help='the port at which the illuminator agent is listening')
    parser.add_argument('-c','--colour',default='red',choices=['red','green','orange'],type=str,help='the default text colour, note that this can be changed inside the text with the \\c escape code, defaults to red')
    parser.add_argument('-s','--speed',default=1,type=float,help='the speed at which the text scrolls, defaults to 1')
    parser.add_argument('-r','--repeat',default=1,type=int,help='the number of times the text repeats, a value of 0 means infinite, defaults to 1')
    parser.add_argument('-f','--flip',default=False,action='store_true',help='horizontally flips the text bitmap')
    parser.add_argument('-d','--direction',default='left',choices=['left','right'],type=str,help='the direction in which the text will scroll, defaults to left')
    parser.add_argument('-t','--type',default='physical',choices=['musical','physical'],type=str,help='the positioning space in which the colours are shown, defaults to musical')
    parser.add_argument('--space',default=1,type=int,help='the spacing between the charactersi, defaults to 1')

    args = parser.parse_args()

    options = DisplayOptions(args.port)
    options.host = args.host
    options.colour = args.colour[:1].upper()
    options.speed = args.speed
    options.repeat = args.repeat
    options.spacing = args.space
    options.flip = args.flip
    options.direction = args.direction
    options.type = args.type

    scroll(args.text, options)
