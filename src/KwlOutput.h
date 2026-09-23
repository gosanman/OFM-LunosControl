#pragma once

#include "IDacDrive.h"
#include "KwlCurve.h"
#include "KwlTypes.h"

namespace Kwl
{
    /// Hoechste Zahl Stellkanaele, die eine Platine dieser Firmware haben darf.
    constexpr uint8_t kOutputChannelsMax = 12;

    // ========================================================================
    // Die einzige Stelle, an der eine Spannung das Geraet verlaesst.
    //
    // Sie haelt drei der Sicherheitsinvarianten fest, und zwar so, dass ein
    // Verstoss nicht kompiliert oder im Test auffaellt, statt erst am Luefter:
    //
    //  3. Startup schreibt zuerst den Bereich, dann den sicheren Zustand. Vor
    //     begin() nimmt diese Klasse KEINEN Sollwert an - setVolt() gibt false
    //     zurueck und schreibt nichts.
    //  6. Der ego wird immer als Paar geschrieben. setEgo() ist der einzige Weg,
    //     einen zweikanaligen Luefter zu stellen, und er benutzt writePair().
    //  8. Vor Neustart sicherer Zustand. safeAll() schreibt jedem Kanal seinen
    //     Stillstandswert - bipolar 5,00 V, unipolar 0 V.
    //
    // 10. Ausfall wird gemeldet, nicht geraten: antwortet ein Chip nicht, bleibt
    //     `ready` falsch, `error` steht auf DacUnreachable, und es geht nichts
    //     hinaus.
    //
    // Das Busformat kommt als Argument herein (aus FANDRV_DAC_LEFT_ALIGNED,
    // Messung P6) und nicht als Makro - sonst waere diese Schicht nicht ohne
    // Board pruefbar, und gerade sie entscheidet ueber Stillstand oder Vollgas.
    // ========================================================================
    class KwlOutput
    {
      public:
        /// Treiber und Busformat anmelden. Setzt die Klasse zurueck; nach attach()
        /// ist sie nicht bereit, bis begin() gelaufen ist.
        void attach(IDacDrive* drive, bool leftAligned);

        /// Geraetetyp eines Kanals. Er entscheidet ueber den sicheren Zustand:
        /// bipolar 5,00 V, unipolar 0 V. Kanaele ohne Angabe gelten als bipolar -
        /// das ist der Zustand, der einen unbekannten Luefter stehen laesst.
        void setChannelType(uint8_t channel, FanType type);

        /// Startsequenz: Adressabfrage, Bereichsregister, sicherer Zustand.
        /// Erst danach nimmt die Klasse Sollwerte an.
        bool begin();

        bool ready() const { return mReady; }
        ErrorCode error() const { return mError; }

        /// Einen einkanaligen Luefter stellen.
        bool setVolt(uint8_t channel, float volt);

        /// Einen ego stellen: beide Motoren in einem Frame. `firstChannel` muss
        /// der erste Kanal eines Chips sein, sonst liegen die Motoren auf zwei
        /// Chips und lassen sich nicht gemeinsam stellen.
        bool setEgo(uint8_t firstChannel, const VoltPair& volt);

        /// Alle Kanaele in den sicheren Zustand. Laeuft auch dann, wenn die Klasse
        /// nicht bereit ist - ein Neustart soll nicht daran scheitern, dass vorher
        /// etwas schiefging.
        bool safeAll();

        /// Zuletzt ausgegebene Spannung eines Kanals, fuer Status und Konsole.
        float lastVolt(uint8_t channel) const;

        /// Passt `firstChannel` als erster Kanal eines Chips?
        bool isPairStart(uint8_t firstChannel) const;

      private:
        bool writeVolt(uint8_t channel, float volt);
        void fail(ErrorCode code);

        IDacDrive* mDrive = nullptr;
        bool mLeftAligned = false;
        bool mReady = false;
        ErrorCode mError = ErrorCode::None;

        FanType mType[kOutputChannelsMax] = {};
        float mLastVolt[kOutputChannelsMax] = {};
    };
} // namespace Kwl
