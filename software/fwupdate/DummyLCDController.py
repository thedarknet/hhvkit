#!/usr/bin/python
# 
# Simple module to control the Sparkfun Serial LCD Modules
# 
import serial

class Controller():
    def __init__(self):
        return

    def connect(self, serialPort, baudRate):
        return True

    def write(self, string):
        return

    def disconnect(self):
        return

    def clear(self):
        return

    def setBacklight(self, brightness):
        return
    
    def setCursorPos(self, pos):
        return

    def cursorMoveRight(self, times = 1):
        return

    def cursorMoveLeft(self, times = 1):
        return

    def cursorScrollRight(self, times = 1):
        return

    def cursorScrollLeft(self, times = 1):
        return

if __name__ == '__main__':
    print('Standalone operation not supported...')
