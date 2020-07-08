#ifndef IR_RECEIVER_H
#define IR_RECEIVER_H

#include "Arduino.h"
#include "StateMachine.h"

namespace IrReceiverUtils
{
    using namespace StateMachineUtils;

    enum ReceiverStateId
    {
        WAITING_FOR_AGC, // Have not yet received automatic gain control (AGC) burst which signals the start of a code/repeat
        RECEIVING_PACKET, // Have received the AGC burst and anywhere between 0 and 31 bits
        RECEIVED_PACKET // Have received a full code (or a repeat burst). Waiting for result to be consumed
    };

    struct IrPacket
    {
        bool IsRepeat;
        unsigned long Code;
    };

    // See https://www.sbprojects.net/knowledge/ir/nec.php
    unsigned long const ZERO_DURATION = 1125UL;
    unsigned long const ONE_DURATION = 2250UL;
    unsigned long const REPEAT_DURATION = 2810UL;
    unsigned long const AGC_DURATION = 5060UL;
    unsigned long const HALF_WINDOW = 80UL; // TODO make this configurable
    byte const BITS_PER_CODE = 32;

    bool WithinWindow(unsigned long const testDuration, unsigned long const windowCentre)
    {
        return testDuration >= (windowCentre - HALF_WINDOW) && testDuration <= (windowCentre + HALF_WINDOW);
    }

    class WaitingForAgcState : public State<ReceiverStateId>
    {
        public:
            ReceiverStateId Tick(unsigned long const deltaMicros)
            {
                return WithinWindow(deltaMicros, AGC_DURATION) ? RECEIVING_PACKET : WAITING_FOR_AGC;
            }

            void OnEnterState() { }
    };

    class ReceivingPacketState : public State<ReceiverStateId>
    {
        private:
            volatile IrPacket & packet;
            byte bitsCaptured = 0;

        public:
            ReceivingPacketState(volatile IrPacket & packet)
                : packet(packet)
            { }

            ReceiverStateId Tick(unsigned long const deltaMicros)
            {
                if (WithinWindow(deltaMicros, ZERO_DURATION))
                {
                    packet.Code *= 2;
                    return (++bitsCaptured == BITS_PER_CODE) ? RECEIVED_PACKET : RECEIVING_PACKET;
                }
                else if (WithinWindow(deltaMicros, ONE_DURATION))
                {
                    packet.Code *= 2;
                    packet.Code++;
                    return (++bitsCaptured == BITS_PER_CODE) ? RECEIVED_PACKET : RECEIVING_PACKET; 
                }
                else if (bitsCaptured == 0 && WithinWindow(deltaMicros, REPEAT_DURATION))
                {
                    packet.IsRepeat = true;
                    return RECEIVED_PACKET;
                }
                else
                {
                    return WAITING_FOR_AGC;
                }
            }

            void OnEnterState()
            {
                packet.Code = 0UL;
                bitsCaptured = 0;
            }
    };

    class ReceivedPacketState : public State<ReceiverStateId>
    {
        private:
            volatile IrPacket const & packet;
            volatile unsigned long & lastCode;
            volatile bool & packetReady;

        public:
            ReceivedPacketState(
                volatile IrPacket const & packet,
                volatile unsigned long & lastCode,
                volatile bool & packetReady)
                : packet(packet)
                , lastCode(lastCode)
                , packetReady(packetReady)
            { }

            ReceiverStateId Tick(unsigned long const)
            {
                return RECEIVED_PACKET;
            }

            void OnEnterState()
            {
                if(!packet.IsRepeat) lastCode = packet.Code;
                packetReady = true;
            }
    };

    /**
     * Interface that allows InputPinIrReceiver references to shed their template parameter
     */
    class IrReceiver
    {
        public:
            /**
             * Attempt to read a data packet from the receiver
             *
             * @param outPacket On successful read, will contain packet data
             *
             * @returns True iff. there was a fully captured data packet
             * that had not previously been read
             */
            virtual bool TryGetPacket(IrPacket & outPacket) = 0;
            bool TryGetPacket()
            {
                IrPacket packet;
                return TryGetPacket(packet);
            }

            /**
             * @returns The last code (non-repeat packet) captured by the receiver
             * Returned value is not valid until at least one packet has been captured
             */
            virtual volatile unsigned long GetLastCode() = 0;
    };

    /**
     * IR Receiver for NEC protocol IR data transmission
     * Attach to an interrupt capable digital input pin
     * which has a 38kHz IR demodulator (e.g. TSOP1838) connected
     *
     * This class does NOT buffer packets. Once a data packet has
     * arrived, the receiver will ignore subsequent packets until
     * one of the TryGetPacket overloads reads the packet
     */
    template <int ReceiverPin> class InputPinIrReceiver :
        private StateMachine<ReceiverStateId>,
        public IrReceiver
    {
        private:
            inline static InputPinIrReceiver<ReceiverPin> instance;

            // These variables are written to inside the interrupt context,
            // but can be read from the main program thread. Therefore,
            // they must be marked volatile, so that the compiler does
            // not naively cache them on the main thread.
            volatile IrPacket packet;
            volatile unsigned long lastCode;
            volatile bool packetReady = false;

            WaitingForAgcState waitingForAgcState;
            ReceivingPacketState receivingPacketState;
            ReceivedPacketState receivedPacketState;

            static void handleInputRise()
            {
                instance.Tick();
            }

            InputPinIrReceiver()
                : StateMachine(WAITING_FOR_AGC, &waitingForAgcState)
                , receivingPacketState(packet)
                , receivedPacketState(packet, lastCode, packetReady)
            { }

        protected:
            State<ReceiverStateId> * GetStateInstance(ReceiverStateId const stateIdentifier) const
            {
                switch(stateIdentifier)
                {
                    case RECEIVING_PACKET: return &receivingPacketState;
                    case RECEIVED_PACKET: return &receivedPacketState;
                    case WAITING_FOR_AGC:
                    default:
                        return &waitingForAgcState;
                }
            }

        public:
            /**
             * Attach the receiver to the input pin via a pin interrupt
             * It is the caller's responsibility to ensure that the provided
             * pin is interrupt capable, configured as an input,
             * and that the interrupt is free. No validation is performed
             *
             * @returns The receiver instance
             */
            static IrReceiver& Attach()
            {
                attachInterrupt(
                    digitalPinToInterrupt(ReceiverPin),
                    handleInputRise,
                    RISING);
                return instance;
            }

            static void Detach()
            {
                detachInterrupt(digitalPinToInterrupt(ReceiverPin));
            }

            bool TryGetPacket(IrPacket & outPacket)
            {
                if (packetReady)
                {
                    outPacket.Code = packet.Code;
                    outPacket.IsRepeat = packet.IsRepeat;
                    SetState(WAITING_FOR_AGC);
                    packetReady = false;
                    return true;
                }
                else return false;
            }

            volatile unsigned long GetLastCode()
            {
                return lastCode;
            }
    };
}

#endif //IR_RECEIVER_H