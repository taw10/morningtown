MorningTown: A silent alarm clock
=================================

Description
-----------

MorningTown is a silent alarm clock system based on a Raspberry Pi Pico.

At 7:15am, it lights a single green LED.  At 8am, it additionally lights a red
LED.  Around midday, it turns them both off.  It does practically nothing else.


Why???
------

Awareness of the exact time is bad for your sleep.  If you're having trouble
getting to sleep, knowing how late it is only increases your stress level,
making it even harder to sleep.  Similarly, knowing that your alarm will go off
shortly can prevent you from relaxing in the morning, and grabbing a valuable
few minutes' extra sleep.

All that you really need is an indicator of whether or not it's a sensible time
to get up, or whether you should go back to sleep.  That's all that MorningTown
will give you.

In contrast to a clock radio or "artificial sunrise" lamp, there is no danger
of disturbing any other beings who share the room and may run on different
schedules.

Obviously, you should set an audible alarm clock for important wake-up calls.
But, in combination with regular sleep hygiene, two LEDs might be all you need
to synchronise your sleep pattern!


Hardware
--------

For a 'deluxe' implementation, send the circuit board design in the `pcb`
folder ([KiCad](https://www.kicad.org/) format) to your manufacturer of choice
(I have been using [Aisler](https://aisler.net/).

For a 'cheap and cheerful' version, simply solder red and green LEDs to GPIOs
22 and 21 respectively of a Raspberry Pi Pico W, each with a 1.5 kOhm series
resistor (you won't need much brightness).  There is a ground pad conveniently
placed between these two pins.

![Circuit diagram](circuit.png)    ![Photo](photo.jpg)

(Hopefully your soldering is a little neater than mine).

You can use other colours if you prefer, of course, or different GPIOs - just
change the definitions of `LED_RED`, `LED_GREEN` and `TEST_BUTTON` in
`firmware/morningtown.c`.

The custom PCB design adds a battery-backed real-time clock chip, which keeps
time when power is removed from the Pi Pico.  With the 'cheap and cheerful'
option, the Pico will instead need to synchronise itself using NTP.


Firmware
--------

Edit `compile` to set the path to the [Pico SDK](https://github.com/raspberrypi/pico-sdk),
as well as your WLAN name and password if you are using a Pico W.

The wake-up times, as well as UTC offsets and DST changeover, are set in
routine `check_clock()` in `morningtown.c`.  By default the calculations are
set for central European time (UTC+1/UTC+2 with changeover on the last Sundays
of March and October).  Unfortunately, a full local time library is far too big
to fit on the Pico, so you will have to figure out the logic yourself for other
time zones.

Run `compile`, then copy `build/morningtown.uf2` to the Pico.


Operation
---------

Connect the Pico to any USB power supply.  The board LED will light briefly,
followed by the red LED (if a DS3231 real-time clock is detected) and green
(to indicate the Pico's RTC is running).  If applicable, the device will then
attempt to connect to the WLAN and synchronise its clock

When the (optional) test button is held, the LEDs indicate as follows:
* Red: NTP synchronised if applicable, otherwise always on.
* Green: RTC time OK (regardless of source).
* Board LED: WLAN connected if applicable, otherwise always on.


Licence
-------

MorningTown is based on a heavily modified version of one of the Pico SDK
examples (picow-ntp-client), copyright (c) 2022 Raspberry Pi (Trading) Ltd.,
under the BSD 3-clause licence.

MorningTown is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later
version.

MorningTown is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
MorningTown.  If not, see <http://www.gnu.org/licenses/>.


About the name
--------------

It's like a railway signal for your sleep:

> Somewhere there is sunshine  
> Somewhere there is day  
> Somewhere there is Morning town  
> Many miles away

- [Malvina Reynolds](https://en.wikipedia.org/wiki/Morningtown_Ride)
