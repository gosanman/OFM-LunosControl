#pragma once

#include "KwlCurve.h"
#include "KwlOutput.h"
#include "KwlTypes.h"
#include "OpenKNX.h"

namespace Kwl
{
    // ========================================================================
    // Ein Luefter. In der ETS ein Kanal des Moduls FAN.
    //
    // Der Kanal treibt, er entscheidet nicht: Stufe und Richtung kommen vom Raum
    // (ueber StageArbiter) und vom Verbund (ueber KwlGroup). Hier wird daraus eine
    // Spannung - und nur hier steht, welcher DAC-Kanal dazu gehoert.
    // ========================================================================
    class KwlFan : public OpenKNX::Channel
    {
      public:
        explicit KwlFan(uint8_t index);

        const std::string name() override { return "KwlFan"; }

        void setup();
        void loop();

        bool isActive() const { return mActive; }
        FanType type() const { return mType; }
        uint8_t dacChannel() const { return mDacChannel; }
        uint8_t roomNo() const { return mRoomNo; }
        uint8_t groupNo() const { return mGroupNo; }
        uint8_t phase() const { return mPhase; }
        uint8_t share() const { return mShare; }
        uint8_t stage() const { return mStage; }
        Direction direction() const { return mDirection; }

        /// Belegt dieser Luefter zwei DAC-Kanaele? Nur der ego tut das.
        uint8_t channelsNeeded() const { return Curve::channelsNeeded(mType); }

        /// Stufe und Richtung ausgeben. `below5vIsSupply` kommt aus dem
        /// Board-Header (Messung M1) und wird nicht hier entschieden.
        bool drive(KwlOutput& output, uint8_t stage, Direction direction,
                   bool below5vIsSupply);

        /// Sicherer Zustand dieses Luefters, unabhaengig von allem anderen.
        bool driveSafe(KwlOutput& output);

        /// Eine Zeile fuer "kwl st".
        void printStatusLine();

      private:
        bool mActive = false;
        FanType mType = FanType::E2_60;
        uint8_t mDacChannel = 0;
        uint8_t mRoomNo = 1;
        uint8_t mGroupNo = 1;
        uint8_t mPhase = 0;
        uint8_t mShare = 100;

        uint8_t mStage = 0;
        Direction mDirection = Direction::Supply;
    };
} // namespace Kwl
