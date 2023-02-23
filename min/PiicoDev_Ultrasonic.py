_A=None
from PiicoDev_Unified import *
compat_str='\nUnified PiicoDev library out of date.  Get the latest module: https://piico.dev/unified \n'
_BASE_ADDRESS=53
_DEVICE_ID=578
_REG_STATUS=8
_REG_FIRM_MAJ=2
_REG_FIRM_MIN=3
_REG_I2C_ADDRESS=4
_REG_RAW=5
_REG_PERIOD=6
_REG_LED=7
_REG_SELF_TEST=9
_REG_WHOAMI=1
def _set_bit(x,n):return x|1<<n
class PiicoDev_Ultrasonic:
	def __init__(self,bus=_A,freq=_A,sda=_A,scl=_A,address=_BASE_ADDRESS,id=_A,minimum=0.0,maximum=100.0):
		try:
			if compat_ind>=1:0
			else:print(compat_str)
		except:print(compat_str)
		self.i2c=create_unified_i2c(bus=bus,freq=freq,sda=sda,scl=scl);self._address=address;self.minimum=minimum;self.maximum=maximum
		if type(id)is list and not all((v==0 for v in id)):assert all((x in[0,1]for x in id))and len(id)==4,'id must be a 4-element list containing ones and zeros';self._address=8+id[0]+2*id[1]+4*id[2]+8*id[3]
		else:self._address=address
		try:
			if self.whoami!=_DEVICE_ID:print('* Incorrect device found at address {}'.format(address))
		except:print("* Couldn't find a device - check switches and wiring")
		self.millimeters_per_microsecond=0.343;self.period_ms=20;self.led=True
	def _read(self,register,length=1):
		try:return self.i2c.readfrom_mem(self.address,register,length)
		except:print(i2c_err_str.format(self.address));return _A
	def _write(self,register,data):
		try:self.i2c.writeto_mem(self.address,register,data)
		except:print(i2c_err_str.format(self.address))
	def _read_int(self,register,length=1):
		data=self._read(register,length)
		if data is _A:return _A
		else:return int.from_bytes(data,'big')
	def _write_int(self,register,integer,length=1):self._write(register,int.to_bytes(integer,length,'big'))
	@property
	def new_sample_available(self):'Returns True when a new range sample is available';status=self._read_int(_REG_STATUS,1);return status&1
	@property
	def round_trip_us(self):
		'Returns the pulse round-trip time in microseconds';trip_time=self._read_int(_REG_RAW,2)
		if trip_time is _A:return float('NaN')
		else:return trip_time
	@property
	def distance_mm(self):'Returns the measured distance in millimeters';return round(self.round_trip_us*self.millimeters_per_microsecond/2)
	@property
	def distance_inch(self):'Returns the measured distance in inches';return self.distance_mm/25.4
	@property
	def address(self):'Returns the currently configured 7-bit I2C address';return self._address
	@property
	def led(self):'Returns the state onboard "Power" LED. `True` / `False`';return bool(self._read_int(_REG_LED))
	@led.setter
	def led(self,x):'control the state onboard "Power" LED. accepts `True` / `False`';self._write_int(_set_bit(_REG_LED,7),int(x));return 0
	@property
	def period_ms(self):'Returns the sample period [milliseconds]';return self._read_int(_REG_PERIOD,length=2)
	@period_ms.setter
	def period_ms(self,period):'Set the sample period [milliseconds].';assert 0<=period<=65535,'period_ms must be between 0(disabled) and 65535';self._write_int(_set_bit(_REG_PERIOD,7),int(period),length=2);return 0
	@property
	def whoami(self):'returns the device identifier';return self._read_int(_REG_WHOAMI,2)
	@property
	def firmware(self):'Returns the firmware version';major=self._read_int(_REG_FIRM_MAJ);minor=self._read_int(_REG_FIRM_MIN);return major,minor
	def setI2Caddr(self,newAddr):x=int(newAddr);assert 8<=x<=119,'address must be >=0x08 and <=0x77';self._write_int(_REG_I2C_ADDRESS,x);self._address=x;sleep_ms(5);return 0
	@property
	def self_test(self):'returns a pass/fail of the self test feature used during production';return self._read_int(_REG_SELF_TEST)