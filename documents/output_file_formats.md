# Register data stream formats

## :loud_sound:AYM

Description

## :loud_sound:PSG with extensions

Description

## :loud_sound:RSF

RSF (Registers Stream Flow) format is smaller than PSG, contains basic text information about file and much easy to stream it using low-power devices such as microcontrollers. This format also has a better data compression ratio than PSG.
<br/>

#### _HEADER:_

Offset|Size|Type|Purpose
-|-|-|- 
00|3|text|Signature "RSF"
03|1|uint8|Version (current is 3) 
04|2|uint16|Frame rate (interrupt frequency) usually 50(Hz) 
06|2|uint16|Offset to registers data 
08|4|uint32|`frameCount` (total frames of the song) 
12|4|uint32|`loopFrame` (frame for loop) 
16|4|uint32|`chipFrequency` (frequency of AY) 
20|X|text, 0|String containing 'Title' 
XX|X|text, 0|String containing 'Author' 
XX|X|text, 0|String containing 'Comment'

#### _REGISTERS DATA:_

First check for 2 special values:

```
​​0xFF – interrupt, dont send registers to the chip (just skip sending one time)
0xFE, XX – number of interrupts XX without changing of registers (just skip sending XX times)
```

Register `R00`-`R0D` values (uint8) that changed from previous interrupt:

```
XX1, XX2, R00, R05, R07, R0B - sequence of register data depending on the register mask XX1, XX2
```

Register mask:

```
XX1 – HI value (uint8) of register mask (bit7 - R07, bit0 - R00)
XX2 – LO value (uint8) of register mask (bit5 - R0D, bit0 - R08, bit7,6 - UNUSED)
```

If register mask bit contains 1, register value should follow the register mask. Example:

```
0x00, 0x03, R00, R01      - XX1 = 00000000 XX2 = 00000011
0x03, 0x02, R01, R08, R09 - XX1 = 00000011 XX2 = 00000010
```

## :loud_sound:VGM and :loud_sound:VGZ

Description

## :loud_sound:VTX

Description

## :loud_sound:YM

Description

### Output file formats comparision

:pencil:Features|:loud_sound:AYM|:loud_sound:PSG|:loud_sound:RSF|:loud_sound:VGM|:loud_sound:VGZ|:loud_sound:VTX|:loud_sound:YM
-|:-:|:-:|:-:|:-:|:-:|:-:|:-:
:bulb: Streamable in real time|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:x:|:x:
:bulb: TurboSound (two chips) support|:heavy_check_mark:|:heavy_check_mark:|:x:|:heavy_check_mark:|:heavy_check_mark:|:x:|:x:
:bulb: AY8930 Expanded mode support|:heavy_check_mark:|:heavy_check_mark:|:x:|:heavy_check_mark:|:heavy_check_mark:|:x:|:x:
:gift: Several compression profiles|:heavy_check_mark:|:x:|:x:|:x:|:x:|:x:|:x:
:gift: Lightweight for decompression|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:x:|:x:|:x:
:gift: Compression ratio relative to PSG|2.78|1.00|1.51|?.??|?.??|?.??|?.??
:information_source: Title and artist info|:heavy_check_mark:|:x:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:
:information_source: Destination chip type info|:heavy_check_mark:|:heavy_check_mark:|:x:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:x:
:information_source: Destination chip clock info|:heavy_check_mark:|:x:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:
:information_source: Destination chip output config|:heavy_check_mark:|:x:|:x:|:x:|:x:|:heavy_check_mark:|:x:
:musical_note: Playback frame rate info|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:
:musical_note: Playback loop frame info|:heavy_check_mark:|:x:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:|:heavy_check_mark:
:musical_note: Playback duration profile|:heavy_check_mark:|:x:|:x:|:x:|:x:|:x:|:x:

# Special formats

## WAV

Description
