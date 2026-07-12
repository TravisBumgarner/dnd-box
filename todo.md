We're going to build DND device. 

It'll have a battery attached.

4 LEDs -> red, green, yellow, blue. 
It'll use an ADXL345.

I imagine the sides will be one color per LED - front left right and back. Top will be off, and bottom will be to charge. Let's make it so you can still set a color while it's charging too.

We'll want a calibration script to set the sides as well. So a "Put it on red, then hit enter" and it'll output a list of config params that can then be uploaded to the device.