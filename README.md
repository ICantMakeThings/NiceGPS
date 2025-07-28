
## NiceGPS

<img width="500" height="500" alt="cover" src="https://github.com/user-attachments/assets/e2f6cade-cc33-49ea-b45f-1a8cac47d903" />


Ah yes, another project with the word "Nice", why? Not because its nice, but because it uses the "Nice!Nano" (nRF52840) as its MCU

Its a simple to make, standalone GPS receiver, showing everything you need to know when in the wild, and hopefully the battery lasts.


#### Links
[My Site](https://icmt.cc/p/nicegps/)

[3D Print](https://www.thingiverse.com/thing:7103307)

## Required Components

+ Nice!Nano: [AliExpress Link](https://s.click.aliexpress.com/e/_oDiDpnE)
+ M10S GPS: [AliExpress Link](https://s.click.aliexpress.com/e/_omVV4ia)
+ 0.96Inch Display: [AliExpress Link](https://s.click.aliexpress.com/e/_ooXwYgq)
+ 6*6 Silicone Switch: [AliExpress Link](https://s.click.aliexpress.com/e/_oDcs8Wa)

*Note: These are referral links. If you purchase through it, I earn a commission at no extra cost to you.*

## But Why?

I go out in big forests, I'd like to have a GPS that's decently cheap to make, for when I communicate on radio with others in the forest, to say where i am to more easily find each other. Will it be practical.. TBD.


##  Connections

Its quite simple, 
2 Buttons connected from:

P0.29 to gnd.

P0.31 to gnd.

I2C for the display
P1.06 (SCL), P1.04 (SDA)

And P0.06 (TX) P0.08 (RX) to the GPS

Yes. Thats all!


## Notes
The full version (With the big antenna on its hat) can fit a battery theoretically 4x50x30mm battery, i say theoretically because i shoved a 30x20x4mm battery.
The small version can hold 25x30xXmm (X because the thickness that will fit depends on the wires you solder on, but about 4mm as well)
