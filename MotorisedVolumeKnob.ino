#include "IrReceiver.h"
#include "VolumeMotorStateMachine.h"

using namespace IrReceiverUtils;
using namespace VolumeMotorUtils;

int const IR_RECV_PIN = 2;
int const VOLUME_UP_PIN = 4;
int const VOLUME_DOWN_PIN = 3;

void setup()
{
    pinMode(IR_RECV_PIN, INPUT);
    pinMode(VOLUME_UP_PIN, OUTPUT);
    pinMode(VOLUME_DOWN_PIN, OUTPUT);
}

auto & receiver = InputPinIrReceiver<IR_RECV_PIN>::Attach(/*inverted:*/true);
auto volumeMotorConfig = VolumeMotorConfig
{
    .VolumeUpCode = 0xFFA857,
    .VolumeDownCode = 0xFFE01F,
    .VolumeUpPin = VOLUME_UP_PIN,
    .VolumeDownPin = VOLUME_DOWN_PIN,
    .BrakeDurationMicros = 100UL * 1000UL,
    .MovementTimeoutMicros = 120UL * 1000UL
};
auto motorStateMachine = VolumeMotorStateMachine(receiver, volumeMotorConfig);

void loop()
{
    motorStateMachine.Tick();
}