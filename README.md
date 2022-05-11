# Reflow Oven

## Solder Paste Reflow Profiles
[this](https://www.compuphase.com/electronics/reflowsolderprofiles.htm) resource describes in detail different solder paste alloys and their reflow profiles.
The most common lead-free solder paste is

## Modifications to AT24Cxx lib
    So the AT24Cxx lib has a poorly written part of it which is that in the constructor it calls `Wire.begin()` which means that you can't make a globally accessible instance. My previous work around was to just have a pointer to a AT24Cxx object within a class that was globally visible, but the issue with this is that some Arduino Cores don't support malloc it seems. Instead of futzing with the Arduino Core I thought it would be easier to just remove the Wire.begin()` call fro the libs coinstructor and break it out into a `begin()` method, which is more idiomatic Arduino anyway. Maybe one day I'll fork this lib and make the change, but for now....ehhh. whatever. im too lazy.
