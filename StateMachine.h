#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

namespace StateMachineUtils
{
    template <class TStateId> class State
    {
        public:
            /**
             * @param deltaMicros The time (in microseconds) since the last tick of the state machine
             * @return The new state of the state machine
             */
            virtual TStateId const Tick(unsigned long const deltaMicros) = 0;

            /**
             * Called when the state machine enters this state.
             * Should reset any internal state inside the State object.
             * Should apply any associated external state (e.g. turning on an output pin),
             * making no assumptions (e.g. do not assume that output pins are already LOW)
             */
            virtual void OnEnterState() = 0;
    };

    unsigned long const Duration(unsigned long const startMicros, unsigned long const endMicros)
    {
        if(startMicros > endMicros) return startMicros - endMicros;
        else return endMicros - startMicros;
    }

    template <class TStateId> class StateMachine
    {
        private:
            State<TStateId> * currentState;
            unsigned long lastTickMicros = 0;
            TStateId currentStateId;

        protected:
            void SetState(TStateId const newStateId)
            {
                if (newStateId != currentStateId)
                {
                    currentState = GetStateInstance(newStateId);
                    currentState->OnEnterState();
                    currentStateId = newStateId;
                }
            }

            /**
             * @param stateId An identifier representing a state
             * @return Pointer to a state object corresponding with the given id
             */
            virtual State<TStateId> * GetStateInstance(TStateId const stateIdentifier) const = 0;

        public:
            StateMachine(
                TStateId const initialStateId,
                State<TStateId> * currentState)
                : currentStateId(initialStateId)
                , currentState(currentState)
            { }

            void Tick()
            {
                auto const currentMicros = micros();
                SetState(currentState->Tick(Duration(lastTickMicros, currentMicros)));
                lastTickMicros = currentMicros;
            }
    };
}

#endif //STATE_MACHINE_H