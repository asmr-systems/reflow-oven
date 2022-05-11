# Debugging
## Touch Screen not working anymore (or intermittently) [SOLVED]
**Observation**:
* when thermocouple is detached & SSRs are attached & powered by USB, Touch screen works after a restart (pressing button), but not a power on
* when thermocouple is re-attached after the arduino has been restarted and touch screen works and SSRs are attached and powered by USB, Touch Screen still works
* when thermocouple & SSRs are attached and it is powered by mains, after a reset Touch Screen doesn't work :(
* after I tested by powering with mains, Touch Screen no longer works under the conditions it just worked under >:(
**Solution**:
It appears that the `touch.begin()` method was non-deterministically returning `false` and so the touch screen
wasn't being initialized properly. This might have to do with some timing stuff with the new platform.
Adding a loop until it returns `true` fixed the problem.
## EEPROM Not Working: I2C Code Hangs on `Wire.endTransmission()`
This might be a connection issue. If the wiring is bad, it will just hang apparently.
