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
        self.serial.write('\x02A'.encode())
        s = self.serial.readline()
        self.assertEqual(s, '\x02A 00\r\n'.encode())

class TestApp(unittest.TestCase):
    def test_nothing(self):
        self.assertTrue(True)

if __name__ == '__main__':
    unittest.main()
