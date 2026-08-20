"""
    cwwrapper_vfi.py

    Authors: Joe Grand (https://joegrand.com) and Lennert Wouters (https://x.com/lennertwo)
    Released as part of https://github.com/joegrand/stm32-fault-injection/
    
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see http://www.gnu.org/licenses/.
"""

import chipwhisperer as cw
from stmbl import *
#from jtagbs import JTAGBS, JTAGCWUserIO
import time
import numpy as np
import itertools
from bokeh.plotting import figure, show
from bokeh.io import output_notebook, push_notebook
from bokeh.layouts import column
from bokeh.palettes import Dark2_5 as palette

class ScopeWrapper:
    def __init__(self, **kwargs):
        self._scope = cw.scope(**kwargs)
        self._scope.default_setup()

        self._scope.io.tio1 = 'serial_tx'
        self._scope.io.tio2 = 'serial_rx'

        self._scope.clock.clkgen_src = 'system'
        self._scope.clock.adc_mul = 1
        self._scope.adc.bits_per_sample = 8
        self._scope.clock.clkgen_freq = 200e6
        self._scope.io.glitch_lp = False
        self._scope.io.glitch_hp = False
        self._scope.io.hs2 = 'disabled'
        self._scope.io.cwe.setTargetPowerSlew(True)

        self._scope.scopetype.usart.init(115200, stopbits=1, parity="even")
        self._ser = self._scope.scopetype.usart
        #self._jtag_interface = JTAGCWUserIO(self._scope)

        self._scope.glitch.enabled = True
        self._scope.glitch.clk_src = 'pll'
        self._scope.glitch.output = "enable_only"
        self._scope.glitch.trigger_src = 'ext_single'
        self._scope.io.glitch_trig_mcx = 'glitch'
        self._scope.glitch.enabled = False
        
        self._downgrade_voltage = 0
        self._downgrade_offset = 0
        self._downgrade_sad_pattern = 0
        self._downgrade_sad_threshold = 0
        self._downgrade_adc_gain = 0



    def __getattr__(self, name):
        return getattr(self._scope, name)

    def __str__(self):
        return str(self._scope)


    def jtag_idcode(self):
        self._jtag_interface.jtag_rawrw([0x0], [0x1F], num_bits=5, write_only=True)
        self._jtag_interface.jtag_rawrw([0x0], [0x02], num_bits=4, write_only=True)
        result = bytes( self._jtag_interface.jtag_rawrw([0]*4, [0]*4, num_bits=32)[0:4][::-1])
        return result.hex()


    def enable_glitch(self):
        self._scope.glitch.enabled = True
        self._scope.glitch.clk_src = 'pll'
        self._scope.glitch.output = "enable_only"
        self._scope.glitch.trigger_src = 'ext_single'
        self._scope.io.glitch_trig_mcx = 'glitch'


    def disable_glitch(self):
        self._scope.glitch.enabled = False


    def adc_trigger(self, level, gain):
        self._scope.gain.db = gain
        self._scope.trigger.module = 'ADC'
        self._scope.trigger.level = level


    def nrst_trigger(self):
        self._scope.gain.db=30
        self._scope.adc.samples = 80000
        self._scope.adc.presamples = 1000
        self._scope.trigger.module = 'basic'
        self._scope.adc.basic_mode = "rising_edge"
        self._scope.trigger.triggers = "nrst"


    def bl_connect(self, delay=0.15):
        self._scope.io.nrst = False
        time.sleep(0.05)
        self._scope.io.pdic = True
        #self._scope.io.nrst = True
        self._scope.io.nrst = "high_z"
        
        time.sleep(delay)
        self._scope.io.pdic = False
        self._ser.flush()
        self._ser.write(bytes([0x7F]))
        ret = bytearray(self._ser.read(1))
        
        if len(ret) == 1 and ret[0] == 0x79:
            return
        else:
            raise ValueError


    def bl_connect_retry(self, n=5):
        for i in range(n):
            try:
                self.bl_connect()
                return
            except:
                time.sleep(0.2*n)
                continue
        raise ValueError


    def bl_write_edge_trigger(self):
        self._scope.trigger.module = 'edge_counter'
        self._scope.trigger.triggers = 'tio1'
        self._scope.trigger.edges = 4


    def bl_go_edge_trigger(self):
        self._scope.trigger.module = 'edge_counter'
        self._scope.trigger.triggers = 'tio1'
        self._scope.trigger.edges = 6


    def bl_read_edge_trigger(self):
        self._scope.trigger.module = 'edge_counter'
        self._scope.trigger.triggers = 'tio1'
        self._scope.trigger.edges = 6


    def sad_trigger_downgrade(self, pattern=None, threshold=2000):
        self._scope.trigger.module = 'SAD'
        self._scope.adc.samples = 30000
        self._scope.adc.presamples = 5000
        
        if pattern is not None:
            self._scope.SAD.threshold = threshold
            self._downgrade_sad_threshold = threshold
            self._scope.SAD.reference = pattern
            self._downgrade_sad_pattern = pattern
            self._downgrade_adc_gain = self._scope.gain.db
        else:
            self._scope.SAD.threshold = self._downgrade_sad_threshold
            self._scope.SAD.reference = self._downgrade_sad_pattern
            self._scope.gain.db = self._downgrade_adc_gain


    def sad_trigger(self, pattern, threshold=2000):
        self._scope.trigger.module = 'SAD'
        self._scope.SAD.threshold = threshold
        self._scope.SAD.reference = pattern
            

    def simple_plot(self, traces, ntraces=5):
        output_notebook()
        p = figure(sizing_mode='scale_width', height=300, width=900)

        x_range = list(range(0, traces.shape[1]))
        colors = itertools.cycle(palette) 

        for i, color in zip(range(ntraces), colors):
            p.line(x_range, traces[i], color=color, legend_label=str(i))

        p.line(x_range, np.mean(traces, axis=0), color='blue', legend_label='mean')
            
        p.legend.click_policy="hide"
        show(p)


    def bl_checksum(self, data):
        if type(data) == int or len(data) == 1:
            if data < 0 or data > 255:
                raise ValueError('Checksum input error:', data)
            return 0xff-data
        else:
            chk = 0
            for d in data:
                chk ^= d
            return chk & 0xff


    @property
    def downgrade_voltage(self):
        return self._downgrade_voltage

    @downgrade_voltage.setter
    def downgrade_voltage(self, voltage):
        self._downgrade_voltage = voltage

    @property
    def downgrade_offset(self):
        return self._downgrade_offset

    @downgrade_offset.setter
    def downgrade_offset(self, offset):
        self._downgrade_offset = offset

    @property
    def downgrade_sad_pattern(self):
        return self._downgrade_sad_pattern

    @downgrade_sad_pattern.setter
    def downgrade_sad_pattern(self, pattern):
        self._downgrade_sad_pattern = pattern

    @property
    def downgrade_sad_threshold(self):
        return self._downgrade_sad_threshold

    @downgrade_sad_threshold.setter
    def downgrade_sad_threshold(self, threshold):
        self._downgrade_sad_threshold = threshold