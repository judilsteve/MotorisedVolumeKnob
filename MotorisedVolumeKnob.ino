#include "IrReceiver.h"
#include "VolumeMotorStateMachine.h"

using namespace IrReceiverUtils;

int const IR_RECV_PIN = 2;

void setup()
{
    Serial.begin(38400);
    pinMode(IR_RECV_PIN, INPUT);
}

IrPacket packet;
auto const & receiver = InputPinIrReceiver<IR_RECV_PIN>::Attach(/*inverted:*/true);

void loop()
{
    if (receiver.TryGetPacket(packet))
    {
        if (packet.IsRepeat) Serial.println("RPT");
        else Serial.println(packet.Code, HEX);
    }
}