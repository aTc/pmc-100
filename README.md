# pmc-100
Utility to  read/write sequencer data from the Philips PMC 100 and convert to midi

## usage

* Save your composition to tape on the PMC 100
* Record the resulting noise to an audio file.  
  I've only really tested 48khz mono wav files, but anything libsndfile supports should work. Example wav included.
* run "pmc in.wav" and it should read the audio file and print the song data in it.
* running "pmc -m out.mid in.wav" should convert the data to a standard midi file.

## todo :

- Reverse engineer the user styles.

With the PMC 100 came a cassette tape with example songs and additional styles that could be loaded into the PMC.
Unfortunately my PMC 100 didn't come with this cassette tape. Please contact me if you have one and can send me a recording.

- There are still some unidentified bytes/bits in the header and song data.
- Better midi output



pmc [options] [input audio file]  

parses and converts audio data from Philips PMC 100  
-m, --midi FILENAME 	 write midi file  
-w, --wav  FILENAME 	 write wav  
-b, --binary FILENAME	 write raw binary data to file  
-p, --print          	 print data read (default)  
-d, --debug          	 output debug info while reading


----
This is really just a quick hack to see if it was possible to reverse engineer the data format used by the PMC, and make the device slightly more useful.

The output midi is pretty basic. It uses channel 0 for the melody notes, and channel 1 for the chord notes. For now chords are only played as continous notes.
Tied notes are output as normal notes.
No attempt is made at replicating the style percussion/chord sequences

The "write wav" option writes back the converted data to a new .wav.
At the moment this is only to test if generating audio and reading it into the PMC works.
Record the resulting .wav to tape, and load it back into the PMC, and it should play the same sequence as the original data.

"write raw binary" strips the start/stop bits from the input

----

Also included is a dump of the PMC-100 rom, in case anyone wants to dig really deep into the workings of the device.
ROM address is from 0x8000 to 0xffff
The start of the rom has a nice message from the creators.
```
DECEMBER 1987 
PMC100 IS BROUGHT TO YOU BY... 
HARDWARE DESIGN: RICHARD WATTS AND LYNDSAY WILLIAMS. 
SOFTWARE: DAVID BAKER, CHRIS MACIEJEWSKI AND MARK PALMER. 
SUPERTECH: CHRIS ROLLAND. 
MUSICAL ARRANGEMENTS: RICK CARDINALI. 
SPECIAL THANKS TO: GABRIEL (BARCLAYCARD) BUTLER AND JOHN VAN TIL. 
MORAL SUPPORT: AKASH, FOSTERS, COMPANY B, DELTA, OUTRUN.    
THAT'S ALL FOLKS!
```

----
The audio data format used by the PMCis similar to the format used by the msx computer.

A 0 bit is represented by a single period of a 1kHz sine wave (actually a filtered square wave that ends up looking more like a sine on tape).

A 1 bit is two periods of a 2kHz sine.

Each data byte is then encoded with 1 start bit (0) , 8 data bits (lsb first) , and 2 stop bits (11).  
Example : 00110000111 -> 0 01100001 11 -> 01100001 -> 134


It starts with a leader of about 5 seconds at 2kHz ( 1 bits), followed by 28 bytes with the header data.
This header has the song length/tempo/style, etc. in it.

Next are the data blocks with the song data, each block is preceded by a shorter leader of about 0.5 seconds at 2Hhz.

Each block is always 256 data bytes long.
```
Header:
byte
00  ([00] * 256 )+[01]: 16 bit data length
01  16 bit data length
02  ?? 
03  accomp style 6=disco, 7=funk, etc. 
04  tempo
05  accomp mode 0=arranged , 1=vari, 2=sustained , 3=off
06	transpose , 5=0, 6=+1 , 3=-2 , range = -5,+6 . finetune=byte-5 , max=11
07	finetune, 34=0 , 35=+1 , 33=-1	, range=-34,+26 , finetune=byte-34 , max=60
08  ?? 
09  ??
10	instr acc1		
11  instr acc2
12  instr acc3
13  instr acc4 
14  instr bass 
15	instr mel
16	vol acc1 ( vol =0x4N , bit 0-3 only) 
17	vol acc2
18 	vol acc3
19	vol acc4
20	vol bass
21	vol mel
22	?? start of melody data (in blocks) [22]*256
23	realtime mode ,0=demo, 1=step, 2=gling 3=supergling , 4=promode 
24  ? data bytes ? ( (( [24]) - 12 ) *256) + [25]  
25  ? data bytes ?
26	steps = ([26]*256)+[27]
27  steps
```
songdata : 

step mode:
In step mode the data is a list of note/chord events in groups of 2 bytes.
If the first byte is 128 (bit 8 set ?) then it's a chord. 

00 0 : rest , 1..127 : note value , 128: chord , 0xff=end of data  
01 note length. if previous byte==128: chord   , 0xff=end of data

chord byte : 0xAB : A=chord, B=root note (0=G)


note lengths:
```
00=1/1    01=1/2    02=1/4    03=1/8   04=1/16    05=1/32
06=1/1.   07=1/2.   08=1/4.   09=1/8.  0a=1/16.     
0c=1/1 3  0d=1/2 3  0e=1/4 3  0f=1/8 3 10=1/16 3  11=1/32 3
	
note is tie when > 0x40 (bit 6)
40=1/1_   41=1/2_   42=1/4_   43=1/8_  44=1/16_   45=1/32_ 
4c=1/1_3  4d=1/2_3  4e=1/4_3  4f=1/8_3 50=1/16_3  51=1/32_3 
```

realtime mode:

In realtime mode, there are two lists, one with chord data, and one with the melody.  
Melody data always seems to start at block 3 (256*3 bytes in ), might be pointed to by header byte [22].

chord list:  
The chord events are in groups of 3 bytes

00 ?? unknown ??  
01 chord byte ( same format as in step mode)  
02 chord length (same as note length in step mode)  

0xff,0xff,0xff = end of data

melody list:  
Same as in step mode, groups of two bytes

00 note (as in step mode)  
01 length (as in step mode)  

0xff,0xff = end of data
