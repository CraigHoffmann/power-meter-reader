# Power Meter Reader

ESP8266 based wifi whole of house electricity meter.  Works by detecting the black mark on the "spinning disk" in the house meter in the main distribution panel. It does this by using an LED that shines on the disk and an LDR to sense the reflected light and trigger a pulse when the reflected light level drops as the black line passes by. As such it is a non-invasive setup that does not require any connections to the mains electricity (except for a suitable power source to run the esp8266 of course)

Uses MQTT for communication with "home assistant"

Setup | Sensor
:----------:|:----------:
![metersetup](https://github.com/CraigHoffmann/power-meter-reader/blob/master/Images/metersetup.jpg?raw=true) | ![sensor](https://github.com/CraigHoffmann/power-meter-reader/blob/master/Images/sensor.jpg?raw=true)
