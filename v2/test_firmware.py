import time
import serial
import unittest

class TestFirmware(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.serial = serial.Serial(
            port='/dev/ttyUSB0',
            timeout=5,
        )
        time.sleep(5)

    @classmethod
    def tearDownClass(cls):
        cls.serial.close()

    def test_echo(self):
        self.assertTrue(True)

    def test_status(self):
        # send reset
        self.serial.write('\x02Z'.encode())
        s = self.serial.readline()
        self.assertEqual(s, '\x02A 00\r\n'.encode())

        # get status
        self.serial.write('\x02A'.encode())
        s = self.serial.readline()
        self.assertEqual(s, '\x02A 00\r\n'.encode())

    def test_reset(self):
        # send reset
        self.serial.write('\x02Z'.encode())
        s = self.serial.readline()
        self.assertEqual(s, '\x02A 00\r\n'.encode())

        # send enable
        self.serial.write('\x02D'.encode())
        s = self.serial.readline()
        self.assertEqual(s, '\x02A 01\r\n'.encode())

        # send reset
        self.serial.write('\x02Z'.encode())
        s = self.serial.readline()
        self.assertEqual(s, '\x02A 00\r\n'.encode())

    def test_enable_disable(self):
        # send enable
        self.serial.write('\x02D'.encode())
        s = self.serial.readline()
        self.assertEqual(s, '\x02A 01\r\n'.encode())

        # send disable
        self.serial.write('\x02C'.encode())
        s = self.serial.readline()
        self.assertEqual(s, '\x02A 00\r\n'.encode())

    def test_send_temp(self):
        # send temp
        self.serial.write('\x02F 123.4'.encode())
        s = self.serial.readline()
        self.assertEqual(s, '\x02F 123.40\r\n'.encode())

    def test_send_temp_slope(self):
        # send temp slope
        self.serial.write('\x02G 5.2'.encode())
        s = self.serial.readline()
        self.assertEqual(s, '\x02G 5.20\r\n'.encode())

    def test_send_duty_cycle(self):
        # send duty cycle
        self.serial.write('\x02H 89'.encode())
        s = self.serial.readline()
        self.assertEqual(s, '\x02H 89\r\n'.encode())

        # send duty cycle above 100
        self.serial.write('\x02H 101'.encode())
        s = self.serial.readline()
        self.assertEqual(s, '\x02H 100\r\n'.encode())

    def test_set_data(self):
        # send duty cycle above 100
        self.serial.write('\x02M 0 this is some custom data!'.encode())
        self.serial.write('\x02N 0'.encode())
        s = self.serial.readline()
        self.assertEqual(s, '\x02N 0 this is some custom data!\r\n'.encode())



class TestApp(unittest.TestCase):
    def test_nothing(self):
        self.assertTrue(True)

if __name__ == '__main__':
    unittest.main()
