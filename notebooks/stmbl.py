"""
    stmbl.py

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

import time
import enum
from tqdm import tqdm

class StmBlException(Exception):
    pass

class NackException(StmBlException):
    pass

class NotConnectedException(StmBlException):
    pass

class UnexpectedResponse(Exception):
    def __init__(self, data, message="Unexpected serial response"):
        self.data = data
        self.message = message
        super().__init__(self.message)

    def __str__(self):
        return f'{self.message}:{self.data.hex()}'

class StmBl:
    # Bootloader command set per AN3155 and AN2606
    @enum.unique
    class Command(enum.IntEnum):
        GET               = 0x00
        GET_VERSION       = 0x01
        GET_ID            = 0x02
        READ_MEMORY       = 0x11
        GO                = 0x21
        WRITE_MEMORY      = 0x31
        ERASE             = 0x43
        EXTENDED_ERASE    = 0x44
        SPECIAL           = 0x50
        EXTENDED_SPECIAL  = 0x51
        WRITE_PROTECT     = 0x63
        WRITE_UNPROTECT   = 0x73
        READOUT_PROTECT   = 0x82
        READOUT_UNPROTECT = 0x92
        GET_CHECKSUM      = 0xA1
        
    
    def __init__(self, ser, debug=False):
        self.ser = ser
        self.ack = 0x79
        self.nack = 0x1F
        self.sync = 0x7F
        self.debug = debug
        self.connected = False


    def _checksum(self, data):
        if type(data) == int or len(data) == 1:
            if data < 0 or data > 255:
                raise ValueError('Checksum input error:', data)
            return 0xff-data
        else:
            chk = 0
            for d in data:
                chk ^= d
            return chk & 0xff
        

    def _connected(self):
        if not self.connected:
            raise NotConnectedException()
            

    def _checkresp(self, data, checkboth=False):
        if data[0] == self.ack:
            if checkboth:
                if data[-1] == self.ack:
                    return True
                elif data[-1] == self.nack:
                    raise NackException()
                else:
                    raise UnexpectedResponse(data)
            else:   
                return True
        if data[0] == self.nack:
            raise NackException()
        elif data[0] != self.ack:
            raise UnexpectedResponse(data)

    def _send_command(self, command):
        cmd_value = int(command)
        self.ser.flush()
        self.ser.write(bytes([cmd_value, self._checksum(cmd_value)]))

    
    def connect(self):
        self.ser.flush()
        self.ser.write(bytes([self.sync]))
        ret = bytearray(self.ser.read(1))
                
        if self.debug:
            print('connect response:', ret.hex(), len(ret))
        
        if self._checkresp(ret) and len(ret) == 1:
            self.connected = True
        else:
            raise UnexpectedResponse(ret)
            

    def get(self):
        self._connected()

        self._send_command(self.Command.GET)
        time.sleep(0.1)
        ret = bytearray(self.ser.read())
        
        if self._checkresp(ret, checkboth=True):
            if self.debug:
                print('Version: 0x%02x' % ret[2])
                print('Supported commands:', ['0x%02x'%i for i in ret[3:-1]])
            
            return ret[1], ret[2], ret[3]
        else:
            # Not reachable?
            raise RuntimeError('Error getting version info:', ret.hex(), len(ret))
    

    def get_version(self):
        self._connected()
        
        self._send_command(self.Command.GET_VERSION)
        ret = bytearray(self.ser.read(5))
        
        if len(ret) == 5 and self._checkresp(ret, checkboth=True):
            if self.debug:
                print('Version: 0x%02x, option 1: %d, option 2: %d' %(ret[1],ret[2], ret[3]))            
            return ret[1], ret[2], ret[3]
        else:
            raise RuntimeError('Error getting version info:', ret.hex(), len(ret))
            

    def read_memory(self, address, nbytes=0xff):
        self._connected()
        
        if nbytes < 0 or nbytes > 255:
            raise ValueError
        
        self._send_command(self.Command.READ_MEMORY)
        ret = bytearray(self.ser.read(1))
        
        if self.debug:
            print('read_memory init:', ret.hex(), len(ret))
        
        if len(ret) == 1 and self._checkresp(ret):
            addr = address.to_bytes(4, 'big')
            addr = addr + bytearray([self._checksum(addr)])
            self.ser.write(addr)
            
            time.sleep(0.05)
            ret = bytearray(self.ser.read())
            
            if self.debug:
                print('read_memory address:', ret.hex(), len(ret))

            if len(ret) != 1 or not self._checkresp(ret):
                raise RuntimeError('Error reading memory after sending address:', ret.hex(), len(ret))
                
            self.ser.write(bytes([nbytes-1, self._checksum(nbytes-1)]))
            #ret = bytearray(self.ser.read(dlen=300, timeout=1000))
            ret = bytearray(self.ser.read(nbytes+1))
            if len(ret) == 0:
                print('Weird state')
                time.sleep(5)
                ret = bytearray(self.ser.read(nbytes+1))
                print('second ret:', ret)
                if len(ret) == 0:
                    raise RuntimeError('Error reading memory:', ret.hex(), len(ret))
            
            if not self._checkresp(ret):
                raise RuntimeError('Error reading memory:', ret.hex(), len(ret))
            
            return ret[1:]
            
        elif len(ret) == 1 and ret[0] == self.nack:
            return -1
        else:
            raise UnexpectedResponse('Error reading memory:', ret)
        

    def read_range(self, start, n):
        blksize = 255
        
        data = bytearray([])
        
        try:
            for i in tqdm(range(n // blksize)):
                blk = self.read_memory((start + (i*blksize)))
                data += blk
                
            i += 1
            if i * blksize != n:
                data += self.read_memory((start + (i*blksize)), n-(i*blksize))
            
        except RuntimeError as e:
            print('Aborting read range:', len(data), i, e)
                
        return data
    

    def write_memory(self, address, data):
        self._connected()
        
        if type(data) != bytearray or len(data) > 256:
            raise TypeError
        
        if len(data) > 256 or len(data) % 4 != 0:
            raise ValueError
            
        self._send_command(self.Command.WRITE_MEMORY)
        ret = bytearray(self.ser.read(1))
        
        if self.debug:
            print('write_memory init:', ret.hex(), len(ret))
        
        if len(ret) == 1 and ret[0] == self.ack:
            addr = address.to_bytes(4, 'big')
            addr = addr + bytearray([self._checksum(addr)])
            self.ser.write(addr)
            ret = bytearray(self.ser.read(1))
            
            if self.debug:
                print('write_memory address:', ret.hex(), len(ret))

            if len(ret) != 1 or ret[0] != self.ack:
                raise RuntimeError('Error writing memory:', ret.hex(), len(ret))
                
            data = bytearray([len(data)-1]) + data
            data = data + bytearray([self._checksum(data)])
            self.ser.write(data)

            ret = bytearray(self.ser.read(1))
            if len(ret) == 0: # Might happen if we write to option bytes and it does a flash erase.
                time.sleep(0.5)
                ret = bytearray(self.ser.read(1))
            
            if ret[0] != self.ack:
                raise RuntimeError('Error writing memory:', ret.hex(), len(ret))

        elif len(ret) == 1 and ret[0] == self.nack:
            raise NackException()
        else:
            raise UnexpectedResponse(ret)
                        

    def write_range(self, start, data, tqd=True):
        blksize = 256
        
        if type(data) != bytearray:
            raise TypeError('Write_range expects a bytearray for the data parameter.')
        
        try:
            if tqd:
                for i in tqdm(range(len(data) // blksize)):
                    self.write_memory((start + (i*blksize)), data[i*blksize:i*blksize+blksize])
            else:
                for i in range(len(data) // blksize):
                    self.write_memory((start + (i*blksize)), data[i*blksize:i*blksize+blksize])
					
            i += 1
            if i * blksize != len(data):
                self.write_memory((start + (i*blksize)), data[i*blksize:])
            
        except RuntimeError as e:
            print('Aborting write range:', len(data), i, e)
        

    def erase_memory(self, pages=None, fullerase=False):
        self._connected()

        if pages == None and fullerase == False:
            raise ValueError('Provide a list of pages to be erased or set fullerase to True.')
        elif fullerase == True and type(pages) == list and len(pages) > 0:
            raise ValueError('Fullerase is enabled and a non empty list of pages is provided. It is not clear what you want to do.')

        self._send_command(self.Command.ERASE)
        ret = bytearray(self.ser.read(1))

        if self._checkresp(ret):
            if fullerase:
                self.ser.write(bytes([0xFF, 0x00]))
                ret = bytearray(self.ser.read(1))
                return self._checkresp(ret)
            else:
                data = bytearray([len(pages)-1]) + bytearray(pages)
                data = data + bytearray([self._checksum(data)])
                self.ser.write(data)
                ret = bytearray(self.ser.read(1))
                return self._checkresp(ret)


    def extended_erase_memory(self, pages=None, fullerase=False):
        self._connected()

        if pages == None and fullerase == False:
            raise ValueError('Provide a list of pages to be erased or set fullerase to True.')
        elif fullerase == True and type(pages) == list and len(pages) > 0:
            raise ValueError('Fullerase is enabled and a non empty list of pages is provided. It is not clear what you want to do.')

        self._send_command(self.Command.EXTENDED_ERASE)
        ret = bytearray(self.ser.read(1))

        if self._checkresp(ret):
            if fullerase:
                data = bytes([0xFF, 0xFF])
                data = data + bytearray([self._checksum(data)])
                self.ser.write(data)
                self.ser.flush()

                while True:
                    ret = bytearray(self.ser.read(1))
                    if len(ret) == 1:
                        if self._checkresp(ret):
                            break
        
                return self._checkresp(ret)
            else:
                data = (len(pages)-1).to_bytes(2, 'big')

                for page in pages:
                    data = data + page.to_bytes(2, 'big')

                data = data + bytearray([self._checksum(data)])

                self.ser.write(data)
                time.sleep(3)
                ret = bytearray(self.ser.read(1, timeout=10))
                return self._checkresp(ret)

    
    def readout_protect(self):
        self._connected()
        self._send_command(self.Command.READOUT_PROTECT)
        ret = bytearray(self.ser.read(1))
        
        if self.debug:
            print('readout protect:', ret.hex())
        
        if len(ret) == 1 and ret[0] == self.ack:
            ret = bytearray(self.ser.read(1))

            if self.debug:
                print('readout protect:', ret.hex())
                
            if len(ret) != 1 and ret[0] != self.ack:
                raise RuntimeError('Error during readout protect:', ret.hex(), len(ret))
        else:
            raise RuntimeError('Error during readout protect:', ret.hex(), len(ret))
        

    def readout_unprotect(self):
        self._connected()
        self._send_command(self.Command.READOUT_UNPROTECT)

        while True:
            data = bytearray(self.ser.read(1))
            if len(data) > 0 and b'y' in data:
                break
    
        while True:
            data = bytearray(self.ser.read(1))
            if len(data) > 0 and b'y' in data:
                break

        return
