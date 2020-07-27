#include "IrReceiver.h"
#include "VolumeMotorStateMachine.h"

using namespace IrReceiverUtils;
using namespace VolumeMotorUtils;

int const IR_RECV_PIN = 2;

void setup()
{
    pinMode(IR_RECV_PIN, INPUT);
}

auto & receiver = InputPinIrReceiver<IR_RECV_PIN>::Attach(/*inverted:*/true);

auto motorStateMachine = VolumeMotorStateMachine(
    receiver,
    VolumeMotorConfig{
        .VolumeUpCode = 0xFFA857,
        .VolumeDownCode = 0xFFE01F,
        .VolumeUpPin = 4,
        .VolumeDownPin = 3,
        .BrakeDurationMicros = 100UL * 1000UL,
        .MovementTimeoutMicros = 100UL * 1000UL
    });

void loop()
{
    motorStateMachine.Tick();
}