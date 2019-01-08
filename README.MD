# VS1053-based music player for the Altronics Mega Box

Refactored from this repo: https://github.com/fnarify/VS1053-Arduino-Music-Player
This library is for using a VS1053-based Arduino shield with an Arduino Uno or Mega. This version in particular is for use with the Altronics Mega Box (K9670).
This library was tested using a Geeetech Arduino MP3 shield board (http://www.geeetech.com/wiki/index.php/Arduino_MP3_shield_board_with_TF_card), which is a pseudo-clone board. The library will likely work with the original SparkFun version as well (https://www.sparkfun.com/products/12660).

Same playback and recording specifications as the original library.
Controlled via 2-4 pushbutton switches on the front, with an optional rotary encoder for volume control; or complete control via an infrared remote control (Altronics A1012 used).
16x2 LCD screen.

**Pin Connections**
```
From    -> To
LCD RS  -> A0
LCD EN  -> A1
LCD D4  -> A2
LCD D5  -> A3
LCD D6  -> A4
LCD D7  -> A5
LCD Backlight -> 5V
IR      -> D3
S1 COM  -> D4
S2 COM  -> D5
S3 COM  -> D14 *
S4 COM  -> D15 *
Encoder Interface A -> D16 *
Encoder Interface B -> D17 *
* Arduino Mega only
```

It needs the libraries:
1. SdFat (https://github.com/greiman/SdFat)
2. SFEMPShield (https://github.com/madsci1016/Sparkfun-MP3-Player-Shield-Arduino-Library)
3. IRremote (https://www.pjrc.com/teensy/td_libs_IRremote.html)
4. Encoder (https://www.pjrc.com/teensy/td_libs_Encoder.html)
I've included most of the libraries as zips incase they differ greatly from the versions found online, or can't be found.

Remember to change the board type in Tools -> Board in the Arduino IDE depending on what type of board you are using.

*VERY IMPORTANT: IF USING ARDUINO MEGA*
The Arduino Mega has a different set of SPI pins than the Arudino Uno, which means they need to be connected in parallel with jumper wires before the program will work. You'll be presented with this error message if not:

*Can't access SD card. Do not reformat.*
No card, wrong chip select pin, or SPI problem?
 SD errorCode: 0X1,0XFF
 
The pin connections are as follows:
```
  Pin    Uno   Mega
  MOSI   11    51
  MISO   12    50
  SCK    13    52
```

So just connect the pins on the left to the ones on the right as stated above and everything should work OK.

Patch files for the VS1053 can be found here: http://www.vlsi.fi/en/support/software/vs10xxpatches.html
I've included most of the patch files required and already converted. You will need patches.053 (required for ogg etc playback) and oggenc.053 (for recording to ogg) located in the root directory of the SD card.

Originally publish in the February 2018 issue of Silicon Chip magazine.
