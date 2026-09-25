#pragma once

#include "MoistAir.h"
#include "SendCondition.h"
#include "OpenKNX.h"
#include "StageArbiter.h"
#include "StageLadder.h"
#include "StageMap.h"
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
    // sie sind ohne Hardware geprueft, dieser Kanal ist es nicht. Was hier steht,
    // ist Verdrahtung - Parameter hinein, Ergebnis hinaus. Jede Regel, die man
    // pruefen koennen muss, gehoert eine Ebene tiefer.
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

        /// Wirksame Stufe dieses Raums samt Herkunft. Das Luefter-Modul liest sie
        /// in-process ab, nie ueber den Bus.
        const StageResult& stage() const { return mStage; }

        /// Welchen Takt dieser Raum wuenscht. Der Verbund entscheidet bei
        /// Uneinigkeit nach seiner eigenen Regel.
        CycleRule cycleWish() const { return mCycleWish; }

        /// Fordert dieser Raum gerade Zuluft an? Solange er Abluft faehrt, muss
        /// anderswo nachstroemen. Die Partner lesen das in-process, nicht ueber
        /// den Bus - das KO gibt es zusaetzlich fuer fremde Geraete.
        bool supplyRequested() const { return mSupplyReq; }

        /// Aussenwerte fuer Raeume, die sie von Raum 1 uebernehmen.
        bool hasOutdoor() const { return mHumOut.valid && mTempOut.valid; }
        float humidityOut() const { return mHumOut.value; }
        float tempOut() const { return mTempOut.value; }

        /// Eine Zeile fuer die Uebersicht.
        void printStatusLine();

        /// Ausfuehrliche Einzelansicht fuer "kwl rNN": warum die Stufe so ist,
        /// wie sie ist. Beim Inbetriebnehmen die eigentliche Frage.
        void printDetail();

      private:
        /// Ein Messwert vom Bus samt Alter. Ein Wert, der nie kam, ist nicht
        /// dasselbe wie einer, der veraltet ist - beide sind aber ungueltig.
        struct Sensor
        {
            float value = 0.0f;
            uint32_t stamp = 0;
            bool valid = false;

            void set(float v, uint32_t now)
            {
                value = v;
                stamp = now;
                valid = true;
            }
        };

        /// Freigaben und Vorgaben der gerade aktiven Betriebsart.
        struct ModeFlags
        {
            bool leadHum;
            bool leadCo2;
            bool leadVoc;
            bool leadTemp;
            bool dehum;
            bool frost;
            bool interval;
            uint8_t runOn;   ///< Nachlauf nach Anforderungsende in Minuten
            uint8_t cycleWish;
        };

        /// Ablauf der Abluftanforderung (Arcus 3.8, die Badlueftung).
        enum class ExhaustState : uint8_t
        {
            Idle,   ///< keine Anforderung
            Lead,   ///< Vorlaufzeit laeuft, noch nichts gefoerdert
            Active, ///< foerdert Abluft
            Lag     ///< Anforderung weg, Nachlaufzeit laeuft
        };

        /// Kein static: die Param-Makros rechnen ueber _channelIndex.
        ModeFlags readModeFlags(OperatingMode mode) const;

        void expireSensors(uint32_t now);
        void runExhaust(const ModeFlags& flags, uint32_t now);
        bool intervalActive(uint32_t now) const;
        uint8_t applyRunOn(uint8_t guidance, uint8_t runOnMinutes, uint32_t now);
        uint8_t runGuidance(OperatingMode mode, uint32_t now);
        void applyCycleWish(uint8_t wish);
        void sendStage();
        void sendMode();
        void sendHumidity();

        bool mActive = false;

        // Sperre: 0 = Stillstand (geht in den Arbiter), 1 = Grundstufe der
        // Betriebsart (wird in loop() begrenzt, damit Rang 1 eine Stelle bleibt).
        bool mLocked = false;
        bool mLockKeepsBase = false;

        /// Ueberwachungszeit der Sensoren in Millisekunden, 0 = keine.
        uint32_t mSensorTimeout = 0;
        /// true: bei fehlenden Messwerten Stillstand, false: Grundstufe weiter.
        bool mStopOnMissing = false;
        bool mSensorsMissing = false;

        /// 0 = ppb (9.008), 1 = Index 5.010, 2 = Index 7.001.
        uint8_t mVocUnit = 0;
        /// Hoehe ueber Meer in Metern, nur fuer die g/kg-Anzeige.
        float mAltitude = 500.0f;

        // Fuehrungen lassen sich einzeln vom Bus abschalten (Arcus Obj 13/16/18).
        bool mLeadHumKo = true;
        bool mLeadCo2Ko = true;
        bool mLeadVocKo = true;
        bool mLeadTempKo = true;

        /// Objekt "Sommer" (1.001): waehlt den langen Zyklus, wenn die Betriebsart
        /// die Entscheidung an dieses Objekt abgibt.
        bool mSummer = false;
        CycleRule mCycleWish = CycleRule::Wrg;

        Sensor mHumIn, mTempIn, mHumOut, mTempOut, mCo2Val, mVocVal, mTempSet;

        StageArbiter mArbiter;
        StageLadder mHumidity;
        StageLadder mCo2;
        StageLadder mVoc;
        StageMap mPercent;
        TempControl mTemp;
        HumidityCompare mCompare;

        // --- Abluftanforderung (Rang 3) --------------------------------------
        bool mExhaustReq = false;
        ExhaustState mExhaust = ExhaustState::Idle;
        uint32_t mExhaustSince = 0;
        uint8_t mExhaustStage = 4;
        uint32_t mExhaustLeadMs = 0;
        uint32_t mExhaustLagMs = 0;
        uint8_t mExhaustIntervalIdx = 0;
        bool mSendSupplyReq = true;
        bool mSupplyReq = false;

        // --- Betriebsweise-KO (Rang 3) ---------------------------------------
        /// 0 = auto, 1 = WRG, 2 = Zuluft, 3 = Abluft.
        uint8_t mDirMode = 0;

        // --- Intervallbetrieb (Rang 6) ---------------------------------------
        uint32_t mIntervalPeriodMs = 0;
        uint32_t mIntervalActiveMs = 0;
        bool mIntervalRunning = false;

        // --- Nachlauf nach Anforderungsende ----------------------------------
        uint8_t mGuidanceHold = 0;
        uint32_t mGuidanceHoldSince = 0;

        // Was zuletzt auf dem Bus war. GroupObject::value() sendet immer, also
        // wird hier entschieden statt dort.
        Sent<uint8_t> mSentStage;
        Sent<uint8_t> mSentPct;
        Sent<uint8_t> mSentMode;
        Sent<uint8_t> mSentDirMode;
        Sent<bool> mSentProtect;
        Sent<bool> mSentDehumBlock;
        Sent<bool> mSentInterval;
        Sent<int32_t> mSentAbsIn;  ///< in 1/100 g/kg, fuer das Totband
        Sent<int32_t> mSentAbsOut;

        StageResult mStage{};
        bool mProtection = false;
        bool mDehumBlocked = false;
    };
} // namespace Kwl
