# MotorisedVolumeKnob

Arduino sketch to control an ALPS RK16812MG0AF (or similar) motorised potentiometer via an L298N (or similar) H-Bridge, TSOP1838 receiver (or similar 38kHz IR demodulator), and NEC protocol IR remote control.

This repo is very much a work in progress. More documentation (both here and in-code) will be available once I get everything working.
For now, I would recommend you look into IRLib2 instead.

## Usage and Configuration

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

You will need to attach an IRReceiver to the interrupt for your desired digital input pin using the templated singleton:

```c++
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

## Troubleshooting

If you find that your volume motor works fine in short bursts, but begins to stutter or stalls when the button is held for longer periods of time, you most likely have a poor quality demodulator. I experienced these issues with a cheap demodulator that was bundled with my remote control. Upgrading to a higher quality demodulator fixed the issue.

## Motivation

I was originally going to use [IRremote](https://github.com/z3t0/Arduino-IRremote) or [IRLib2](https://github.com/cyborg5/IRLib2) for this project, however I found that no matter how I configured my receiver, the remote control for my Panasonic air conditioner would interfere with it, causing the IR receiver object to return a blank code every time I checked for a code until the Arduino was rebooted. So I built the simplest possible NEC protocol decoder that I could, to make my receiver resilient against interference. With this library, the air conditioner remote control is ignored as desired.