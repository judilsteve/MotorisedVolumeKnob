#ifndef VOLUME_MOTOR_STATE_MACHINE_H
#define VOLUME_MOTOR_STATE_MACHINE_H

#include "Arduino.h"
#include "StateMachine.h"
#include "IrReceiver.h"

namespace VolumeMotorUtils
{
    using namespace IrReceiverUtils;
    using namespace StateMachineUtils;

    struct VolumeMotorConfig
    {
        // IR code to signal volume up command
        unsigned long const VolumeUpCode;
        // IR code to signal volume down command
        unsigned long const VolumeDownCode;

        // Digital output pin to drive motor in volume up direction
        int const VolumeUpPin;
        // Digital output pin to drive motor in volume down direction
        int const VolumeDownPin;

        // Duration to drive motor in brake mode (both inputs on) when stopping
        unsigned long const BrakeDurationMicros;
        // Duration to wait since last IR code before stopping
        unsigned long const MovementTimeoutMicros;
    };

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
            VolumeMotorConfig const & config;

        public:
            IdleMotorState(
                IrReceiver & irReceiver,
                VolumeMotorConfig const & config)
                : irReceiver(irReceiver)
                , config(config)
            { }

            MotorStateId const Tick(unsigned long const)
            {
                IrPacket packet;
                if (irReceiver.TryGetPacket(packet) && !packet.IsRepeat)
                {
                    if (packet.Code == config.VolumeUpCode) return VOLUME_INCREASING;
                    else if (packet.Code == config.VolumeDownCode) return VOLUME_DECREASING;
                }
                return IDLE;
            }

            void OnEnterState()
            {
                digitalWrite(config.VolumeUpPin, LOW);
                digitalWrite(config.VolumeDownPin, LOW);
            }
    };

    class BrakingMotorState : public State<MotorStateId>
    {
        private:
            IrReceiver & irReceiver;
            VolumeMotorConfig const & config;
            unsigned long brakeTimeMicros = 0; // Time that motor has been braking for

        public:
            BrakingMotorState(
                IrReceiver & irReceiver,
                VolumeMotorConfig const & config)
                : irReceiver(irReceiver)
                , config(config)
            { }

            MotorStateId const Tick(unsigned long const deltaMicros)
            {
                if (irReceiver.TryGetPacket())
                {
                    // Use last code so that the motor will restart in its last direction if a repeat
                    // packet was missed for some reason (often happens with poor quality demodulators)
                    auto const code = irReceiver.GetLastCode();
                    if (code == config.VolumeUpCode) return VOLUME_INCREASING;
                    else if (code == config.VolumeDownCode) return VOLUME_DECREASING;
                }
                brakeTimeMicros += deltaMicros;
                if(brakeTimeMicros >= config.BrakeDurationMicros) return IDLE;
                else return BRAKING;
            }

            void OnEnterState()
            {
                brakeTimeMicros = 0;
                digitalWrite(config.VolumeUpPin, HIGH);
                digitalWrite(config.VolumeDownPin, HIGH);
            }
    };

    template <bool const VolumeUp> class MovingMotorState : public State<MotorStateId>
    {
        private:
            IrReceiver & irReceiver;
            VolumeMotorConfig const & config;
            unsigned long microsSinceLastForwardCommand = 0; // Time since last matching command/repeat packet

            unsigned long const forwardCommandCode = VolumeUp ? config.VolumeUpCode: config.VolumeDownCode;
            unsigned long const reverseCommandCode = VolumeUp ? config.VolumeDownCode : config.VolumeUpCode;
            int const forwardPin = VolumeUp ? config.VolumeUpPin: config.VolumeDownPin;
            int const reversePin = VolumeUp ? config.VolumeDownPin : config.VolumeUpPin;
            static MotorStateId const forwardState = VolumeUp ? VOLUME_INCREASING : VOLUME_DECREASING;
            static MotorStateId const reverseState = VolumeUp ? VOLUME_DECREASING : VOLUME_INCREASING;

        public:
            MovingMotorState(
                IrReceiver & irReceiver,
                VolumeMotorConfig const & config)
                : irReceiver(irReceiver)
                , config(config)
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

                return microsSinceLastForwardCommand > config.MovementTimeoutMicros ? BRAKING : forwardState;
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
            VolumeIncreasingMotorState(
                IrReceiver & irReceiver,
                VolumeMotorConfig const & config)
                : MovingMotorState(irReceiver, config)
            { }
    };

    class VolumeDecreasingMotorState : public MovingMotorState<false>
    {
        public:
            VolumeDecreasingMotorState(
                IrReceiver & irReceiver,
                VolumeMotorConfig const & config)
                : MovingMotorState(irReceiver, config)
            { }
    };

    class VolumeMotorStateMachine : public StateMachine<MotorStateId>
    {
        private:
            IrReceiver & irReceiver;
            VolumeMotorConfig const config;
            VolumeIncreasingMotorState volumeIncreasingMotorState;
            VolumeDecreasingMotorState volumeDecreasingMotorState;
            BrakingMotorState brakingMotorState;
            IdleMotorState idleMotorState;

        protected:
            State<MotorStateId> * GetStateInstance(MotorStateId const stateId) const
            {
                switch(stateId)
                {
                    case VOLUME_INCREASING: return &volumeIncreasingMotorState;
                    case VOLUME_DECREASING: return &volumeDecreasingMotorState;
                    case BRAKING: return &brakingMotorState;
                    case IDLE:
                    default:
                        return &idleMotorState;
                }
            }

        public:
            VolumeMotorStateMachine(
                IrReceiver & irReceiver,
                VolumeMotorConfig const && inConfig) // Called "inConfig" to distinguish it from the member "config" when initialising the states below
                : StateMachine(IDLE, &idleMotorState)
                , config(inConfig)
                , irReceiver(irReceiver)
                , volumeIncreasingMotorState(irReceiver, config)
                , volumeDecreasingMotorState(irReceiver, config)
                , brakingMotorState(irReceiver, config)
                , idleMotorState(irReceiver, config)
            { }
    };
}

#endif //VOLUME_MOTOR_STATE_MACHINE_H