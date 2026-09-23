#pragma once

#include "MoistAir.h"
#include "OpenKNX.h"
#include "StageArbiter.h"
#include "StageLadder.h"
#include "TempControl.h"

namespace Kwl
{
    // ========================================================================
    // Ein Raum. In der ETS ein Kanal des Moduls ROOM.
    //
    // Hier sitzt alles, was aus Messwerten und Bedienung eine Stufe macht: die
    // Fuehrungen (rF, CO2, VOC, Temperatur), der Feuchtevergleich und die
    // Vorfahrt. Der Raum treibt nichts - er liefert einen Stufenwunsch, den der
    // Verbund und der Luefter weiterverarbeiten.
    //
    // Die Rechenschichten sind bewusst eigene Klassen und hier nur zusammengesetzt:
    // sie sind ohne Hardware geprueft, dieser Kanal ist es nicht.
    // ========================================================================
    class KwlRoom : public OpenKNX::Channel
    {
      public:
        explicit KwlRoom(uint8_t index);

        const std::string name() override { return "KwlRoom"; }

        void setup();
        void loop();
        void processInputKo(GroupObject& ko);

        bool isActive() const { return mActive; }

        /// Wirksame Stufe dieses Raums samt Herkunft.
        StageResult stage() const { return mArbiter.result(); }

        /// Eine Zeile fuer "kwl r".
        void printStatusLine();

      private:
        bool mActive = false;

        StageArbiter mArbiter;
        StageLadder mHumidity;
        StageLadder mCo2;
        StageLadder mVoc;
        TempControl mTemp;
        HumidityCompare mCompare;
    };
} // namespace Kwl
