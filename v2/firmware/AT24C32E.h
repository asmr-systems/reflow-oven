#ifndef AT24C32E_H
#define AT24C32E_H

#include <Wire.h>


class AT24C32E {
public:
    AT24C32E(uint8_t dev_addr = 0) {
        Wire.begin();
        this->dev_addr = 0xA0 | (0x0F & dev_addr);
    };

    uint8_t read(uint16_t address) {
        Wire.beginTransmission(this->dev_addr);

        Wire.write(highByte(address));
        Wire.write(lowByte(address));

        Wire.endTransmission();
        delay(10);

        Wire.requestFrom(this->dev_addr, 1);
        delay(10);

        uint8_t data = Wire.read();
        delay(10);

        return data;
    };

    void read(uint16_t address, uint8_t length, uint8_t *out) {
        // there is a sequential read, but skipping that for now.
        for (uint8_t i = 0; i < length; i++) {
            out[i] = read(address + i);
        }
    };

    void write(uint16_t address, uint8_t data) {
        Wire.beginTransmission(this->dev_addr);

        Wire.write(highByte(address));
        Wire.write(lowByte(address));

	Wire.write(data);

	delay(10);

	Wire.endTransmission();
	delay(10);
    };

    void write(uint16_t address, uint8_t *data, uint8_t length) {
        for (uint8_t i = 0; i < length; i++) {
            write(address, data[i]);
        }
    };

private:
    uint8_t dev_addr;
    const uint8_t eeprom_size = 32;
};

#endif
