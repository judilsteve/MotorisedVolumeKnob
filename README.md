# MotorisedVolumeKnob

Adjust the volume of your stereo system in style! Turn the knob yourself or sit back and let the IR remote control do it for you, the choice is yours with MotorisedVolumeKnob. Ideal for powered studio monitors, typically sold as individual units, with each speaker having its own amp and volume knob. No more running back and forth between your speakers to match the volume settings every time you need to make an adjustment.

Included in this repo:
 - Parts list (scroll down)
 - STLs for various 3D printable tools/templates/parts to help you build this thing
 - Arduino code to make it all work

## Parts list

I have linked the exact listings that I purchased here, but almost all of these parts are available from many suppliers with stock in lots of different regions. Shop around to find local listings and save on shipping.

 - **[Motorised potentiometer:](https://au.mouser.com/ProductDetail/ALPS/RK16812MG0AF?qs=sGAEpiMZZMtC25l1F4XBUza7emsVrz94iYvX1a0%2Fw%2F8%3D)**
    - **Alternatives:** Just buy this one.
    - **Notes:** Alps is supposedly known for high quality audio potentiometers with smooth actuation and good channel matching. I can confirm that to be the case with these. I wouldn't substitute a different unit here, especially given that other brands don't seem to be much cheaper anyway.

 - **[Enclosure:](https://www.aliexpress.com/item/4000207240540.html)**
    - **Alternatives:** There are too many of these available to list. Plastic ones will be cheaper, or you could even 3D print one quite easily.
    - **Notes:** I wouldn't try to cram these components into anything much smaller than this. If you go with a pre-made enclosure, make a template (STLs available in the repo for the linked enclosure) for drilling pilot holes, and then use a file to bring things to final dimension. It will end up looking much cleaner.

 - **[Knob:](https://www.aliexpress.com/item/32749196662.html)**
    - **Alternatives:** You can use any 6mm shaft knob with a grub screw or a notch (often called "D-Shaft" in listings). 3D printing is also an option here (STL available in the repo).
    - **Notes:** Being solid aluminium, this knob has a nice weighty feel to it and the machining is of very high quality. It also seems to be available in other colours if you're not a fan of red. A lot of the cheaper "aluminium" knobs on sale are actually a thin aluminium cylinder wrapped around an injection moulded plastic core. `[Gap between knob and faceplate] = [potentiometer shaft length] - [knob shaft hole depth] - [faceplate thickness] - [spacer thickness]`. For the parts listed here, that works out to `20mm - \~13.7mm - \~1.2mm - 3mm = \~2mm`. Note that your spacer can be no thicker than 3mm, or else you won't have enough thread on the faceplate side of the shaft for you to thread the locking nut onto.

 - **[RCA sockets:](https://www.aliexpress.com/item/33002834825.html)**
    - **Alternatives:** You can definitely get cheaper, but the gold plating looked super cool. I doubt it makes any audible difference to sound quality, though.
    - **Notes:** You'll need 2 of these 2-packs. Don't solder the positive terminal of these until *after* you have mounted them in the enclosure and put the ground ring in place.

 - **[Motor controller:](https://www.aliexpress.com/item/32925603735.html)**
    - **Alternatives:** Anything based on the L298 chip will work great.
    - **Notes:** Cheap as chips and "brakes" the motor if you set both inputs high at the same time.

 - **[Arduino Nano:](https://www.aliexpress.com/item/32341832857.html)**
    - **Alternatives:** Use any microcontroller you like, but the code here is written for Arduino.
    - **Notes:** You can get away with the cheaper 168P chip for this application, and you don't need the header pins either. Just make sure whichever listing you buy specifies that the bootloader is already installed, or you will be in for a fun time to get it up and running.

 - **[IR Remote:](https://www.aliexpress.com/item/1129167188.html)**
    - **Alternatives:** Basically any IR remote you like. Even one for an old TV you have just lying around. Just make sure you get a receiver with a matching carrier frequency. The code in this repo only works with [NEC protocol](https://www.sbprojects.net/knowledge/ir/nec.php) remotes, but Arduino libraries like [IRLib2](https://github.com/cyborg5/IRLib2/tree/master/IRLib2) should have you covered for other protocols.
    - **Notes:** This one came with battery, which is great. It uses the NEC protocol, which is widely supported by many Arduino libraries. It also includes a matching demodulating receiver, but I found that the receiver would miss repeat pulses and the motor would stutter, so I ended up swapping in a more expensive receiver (see below).

 - **[IR Receiver:](https://www.jaycar.com.au/5mm-infrared-receiver/p/ZD1952)**
    - **Alternatives:** Beware cheap receivers. The cheap ones I tried worked fine for about one second of continuous button-holding and then started to miss IR pulses, causing the motor to stutter. The remote above requires a receiver tuned to a 37.9kHz (often listed as 38kHz) carrier frequency.
    - **Notes:** You can get larger modules that combine multiple receivers to allow your remote to work from wider angles and longer distances.

## STLs

See the "STLs" folder for:

 - Printable templates to help you drill/file holes for the aluminium enclosure in the parts list above
 - Spanners for tightening the panel mount RCA jacks (you'll need two of these, one for each side)
 - A spacer for the potentiometer shaft to move the knob as close as possible to the faceplate
 - A mounting bracket for the Arduino Nano

## Arduino code

### Usage and Configuration

In `setup()`, ensure that your pins are configured correctly:

```c++
int const IR_RECV_PIN = 2;
int const VOLUME_UP_PIN = 4;
int const VOLUME_DOWN_PIN = 3;

void setup()
{
    pinMode(IR_RECV_PIN, INPUT);
    pinMode(VOLUME_UP_PIN, OUTPUT);
    pinMode(VOLUME_DOWN_PIN, OUTPUT);
}
```

You will need to attach an IRReceiver to the pin-level interrupt for your desired digital input pin using the templated singleton:

```c++
// Most IR demodulators that I've come across invert the demodulated signal
// That is, the input pin goes LOW when the receiver is detecting a carrier pulse
// Therefore, you likely want to set the 'inverted' parameter here to true
auto & receiver = InputPinIrReceiver<IR_RECV_PIN>::Attach(/*inverted:*/true);
```

Then create a motor state machine that reads your receiver, supplying an appropriate configuration:

```c++
auto motorStateMachine = VolumeMotorStateMachine(
    receiver,
    VolumeMotorConfig{
        // IR code for the remote button that you want to map to volume +
        .VolumeUpCode = 0xFFA857,
        // IR code for the remote button that you want to map to volume -
        .VolumeDownCode = 0xFFE01F,
        // Output pin that, if set HIGH, will drive the motor to increase the volume
        .VolumeUpPin = 4,
        // Output pin that, if set HIGH, will drive the motor to decrease the volume
        .VolumeDownPin = 3,
        // Duration that both output pins will be set HIGH to "brake" the motor when it
        // is set to idle. See here for more info: https://en.wikipedia.org/wiki/H-bridge#Operation
        .BrakeDurationMicros = 100UL * 1000UL,
        // Duration that the state machine will wait between IR remote packets before
        // assuming that the remote button has been released and stopping the motor
        // Note: In the standard NEC protocol, repeat pulses are spaced 110ms apart,
        // so setting the timeout to a value less than this will likely cause the motor
        // to stutter
        .MovementTimeoutMicros = 120UL * 1000UL
    });
```

Finally, place a call to `motorStateMachine.tick()` at the top level of your `loop()` function:

```c++
void loop()
{
    motorStateMachine.Tick();
}
```

### Troubleshooting

If you find that your volume motor works fine in short bursts, but begins to stutter or stalls when the button is held for longer periods of time, you most likely have a poor quality IR receiver/demodulator. I experienced these issues with a cheap demodulator that was bundled with my remote control. Upgrading to a higher quality demodulator fixed the issue.

### Motivation

I was originally going to use [IRremote](https://github.com/z3t0/Arduino-IRremote) or [IRLib2](https://github.com/cyborg5/IRLib2) for this project, however I found that no matter how I configured my receiver, the remote control for my Panasonic air conditioner would interfere with it, causing the IR receiver object to return a blank code every time I checked for a code until the Arduino was rebooted. So I built the simplest possible NEC protocol decoder that I could, to make my receiver resilient against interference. With my library, the air conditioner remote control is ignored as desired.