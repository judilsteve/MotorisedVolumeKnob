#ifndef VOLUME_MOTOR_STATE_MACHINE_H
#define VOLUME_MOTOR_STATE_MACHINE_H

#include "Arduino.h"
#include "StateMachine.h"
#include "IrReceiver.h"

namespace VolumeMotorStateMachine
{
    using namespace IrReceiverUtils;
    using namespace StateMachineUtils;

    // TODO Make all these values configurable
    unsigned long const VOLUME_UP_CODE = 0xFFA857;
    unsigned long const VOLUME_DOWN_CODE = 0xFFE01F;

    int const MOTOR_VOLUME_UP_PIN = 4;
    int const MOTOR_VOLUME_DOWN_PIN = 3;

    enum MotorStateId
    {
        IDLE,
        VOLUME_INCREASING,
        VOLUME_DECREASING,
        BRAKING
    };

    class IdleMotorState : public State<MotorStateId>
    {
        private:
            IrReceiver & irReceiver;

        public:
            IdleMotorState(IrReceiver & irReceiver)
                : irReceiver(irReceiver)
            { }

            MotorStateId const Tick(unsigned long const)
            {
                IrPacket packet;
                if (irReceiver.TryGetPacket(packet) && !packet.IsRepeat)
                {
                    if (packet.Code == VOLUME_UP_CODE) return VOLUME_INCREASING;
                    else if (packet.Code == VOLUME_DOWN_CODE) return VOLUME_DECREASING;
                }
                return IDLE;
            }

            void OnEnterState()
            {
                digitalWrite(MOTOR_VOLUME_UP_PIN, LOW);
                digitalWrite(MOTOR_VOLUME_DOWN_PIN, LOW);
            }
    };

    class BrakingMotorState : public State<MotorStateId>
    {
        private:
            IrReceiver & irReceiver;
            unsigned long const brakeDurationMicros = 60UL * 1000UL; // Time to spend braking before going idle TODO Make configurable
            unsigned long brakeTimeMicros = 0; // Time that motor has been braking for

        public:
            BrakingMotorState(IrReceiver & irReceiver)
                : irReceiver(irReceiver)
            { }

            MotorStateId const Tick(unsigned long const deltaMicros)
            {
                if (irReceiver.TryGetPacket())
                {
                    // Use last code so that the motor will restart in its last direction if the repeat
                    // packet was a little late for some reason (usually voltage sag in remote battery)
                    // TODO Make configurable
                    auto const code = irReceiver.GetLastCode();
                    if (code == VOLUME_UP_CODE) return VOLUME_INCREASING;
                    else if (code == VOLUME_DOWN_CODE) return VOLUME_DECREASING;
                }
                brakeTimeMicros += deltaMicros;
                if(brakeTimeMicros >= brakeDurationMicros) return IDLE;
                else return BRAKING;
            }

            void OnEnterState()
            {
                brakeTimeMicros = 0;
                digitalWrite(MOTOR_VOLUME_UP_PIN, HIGH);
                digitalWrite(MOTOR_VOLUME_DOWN_PIN, HIGH);
            }
    };

    template <bool const VolumeUp> class MovingMotorState : public State<MotorStateId>
    {
        private:
            IrReceiver & irReceiver;
            unsigned long const timeoutMicros = 120UL * 1000UL; // TODO Make configurable
            unsigned long microsSinceLastForwardCommand = 0; // Time since last matching command/repeat packet

            static unsigned long const forwardCommandCode = VolumeUp ? VOLUME_UP_CODE : VOLUME_DOWN_CODE;
            static unsigned long const reverseCommandCode = VolumeUp ? VOLUME_DOWN_CODE : VOLUME_UP_CODE;
            static int const forwardPin = VolumeUp ? MOTOR_VOLUME_UP_PIN : MOTOR_VOLUME_DOWN_PIN;
            static int const reversePin = VolumeUp ? MOTOR_VOLUME_DOWN_PIN : MOTOR_VOLUME_UP_PIN;
            static MotorStateId const forwardState = VolumeUp ? VOLUME_INCREASING : VOLUME_DECREASING;
            static MotorStateId const reverseState = VolumeUp ? VOLUME_DECREASING : VOLUME_INCREASING;

        public:
            MovingMotorState(IrReceiver & irReceiver)
                : irReceiver(irReceiver)
            { }

            MotorStateId const Tick(unsigned long const deltaMicros)
            {
                IrPacket packet;
                if (irReceiver.TryGetPacket(packet))
                {
                    if (packet.IsRepeat || packet.Code == forwardCommandCode) microsSinceLastForwardCommand = 0;
                    else if (packet.Code == reverseCommandCode) return reverseState;
                }
                else microsSinceLastForwardCommand += deltaMicros;

                return microsSinceLastForwardCommand > timeoutMicros ? BRAKING : forwardState;
            }

            void OnEnterState()
            {
                microsSinceLastForwardCommand = 0;
                // Setting the reverse pin to low first ensures that no braking occurs
                digitalWrite(reversePin, LOW);
                digitalWrite(forwardPin, HIGH);
            }
    };

    class VolumeIncreasingMotorState : public MovingMotorState<true>
    {
        public:
            VolumeIncreasingMotorState(IrReceiver & irReceiver)
                : MovingMotorState(irReceiver)
            { }
    };

    class VolumeDecreasingMotorState : public MovingMotorState<false>
    {
        public:
            VolumeDecreasingMotorState(IrReceiver & irReceiver)
                : MovingMotorState(irReceiver)
            { }
    };

    class VolumeMotorStateMachine : public StateMachine<MotorStateId>
    {
        private:
            IrReceiver & irReceiver;
            VolumeIncreasingMotorState volumeIncreasingMotorState;
            VolumeDecreasingMotorState volumeDecreasingMotorState;
            BrakingMotorState brakingMotorState;
            IdleMotorState idleMotorState;

        protected:
            State<MotorStateId> & GetStateInstance(MotorStateId const stateId)
            {
                switch(stateId)
                {
                    case VOLUME_INCREASING: return volumeIncreasingMotorState;
                    case VOLUME_DECREASING: return volumeDecreasingMotorState;
                    case BRAKING: return brakingMotorState;
                    case IDLE:
                    default:
                        return idleMotorState;
                }
            }

        public:
            VolumeMotorStateMachine(IrReceiver & irReceiver)
                : StateMachine(IDLE, &idleMotorState)
                , irReceiver(irReceiver)
                , volumeIncreasingMotorState(irReceiver)
                , volumeDecreasingMotorState(irReceiver)
                , brakingMotorState(irReceiver)
                , idleMotorState(irReceiver)
            { }
    };
}

#endif //VOLUME_MOTOR_STATE_MACHINE_H