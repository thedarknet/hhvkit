#!/usr/bin/python
# 
# Simple module to control the Sparkfun Serial LCD Modules
# 
import serial

# 
# LCD Constants
# 
LCD_BACKLIGHT_OFF = 128
LCD_BACKLIGHT_MAX = 157
LCD_CHARS = 16
LCD_LINES = 2
LCD_LINESTART = [0, 64, 16, 80] # Cursor position starts for 16 character displays
LCD_MAX_CHARS = LCD_CHARS * LCD_LINES

# 
# Control characters/commands
# 
LCD_BACLKIGHT_CMD = 0x7C
LCD_CMD = 0xFE
LCD_CMD_CLEAR = 0x01
LCD_CMD_CURSORPOS = 0x80
LCD_CMD_MOVRIGHT = 0x14
LCD_CMD_MOVLEFT = 0x10
LCD_CMD_SCRLRIGHT = 0x1C
LCD_CMD_SCRLLEFT = 0x18

class Controller():
    def __init__(self):
        self.backlight = 1.0
        self.connected = False
        self.serialPort = None

    def connect(self, serialPort, baudRate):
        if self.serialPort is not None and self.serialPort.isOpen():
            self.serialPort.close()

        self.serialPort = serial.Serial(port = serialPort, baudrate = baudRate)
        self.serialPort.close()
        self.serialPort.open()

        if self.serialPort.isOpen():
            self.connected = True
            return True
        else:
            self.connected = False
            return False

    def write(self, string):
        if self.connected:
            self.serialPort.write(string)

    def disconnect(self):
        if self.connected:
            self.serialPort.close()

    def clear(self):
        self.write(chr(LCD_CMD) + chr(LCD_CMD_CLEAR))
        self.write(chr(LCD_CMD) + chr(LCD_CMD_CLEAR))

    def setBacklight(self, brightness):
        if brightness < 0:
            brightness = 0
        elif brightness > 1:
            brightness = 1
        
        self.backlight = brightness
        LCD_newBrightness = int(self.backlight * (BACKLIGHT_MAX - LCD_BACKLIGHT_OFF)) + LCD_BACKLIGHT_OFF
        self.write(chr(LCD_BACLKIGHT_CMD) + chr(newBrightness))
    
    def setCursorPos(self, pos):
        if pos < 0:
            pos = 0
        elif pos > (LCD_MAX_CHARS - 1):
            pos = (LCD_MAX_CHARS - 1)

        line = int(pos)/int(LCD_CHARS)
        lineStart = LCD_LINESTART[line]
        linePos = pos - line * LCD_CHARS + lineStart

        self.write(chr(LCD_CMD) + chr(LCD_CMD_CURSORPOS + int(linePos)))

    def cursorMoveRight(self, times = 1):
        for dummy in range(times):
            self.write(chr(LCD_CMD) + chr(LCD_CMD_MOVRIGHT))    

    def cursorMoveLeft(self, times = 1):
        for dummy in range(times):
            self.write(chr(LCD_CMD) + chr(LCD_CMD_MOVLEFT))

    def cursorScrollRight(self, times = 1):
        for dummy in range(times):
            self.write(chr(LCD_CMD) + chr(LCD_CMD_SCRLRIGHT))   

    def cursorScrollLeft(self, times = 1):
        for dummy in range(times):
            self.write(chr(LCD_CMD) + chr(LCD_CMD_SCRLLEFT))

if __name__ == '__main__':
    print('Standalone operation not supported...')
