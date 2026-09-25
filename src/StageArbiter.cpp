#include "StageArbiter.h"

namespace Kwl
{
    namespace
    {
        // Vorgaben aus PLAN Phase 4a, Tabelle der sieben Betriebsarten.
        // Index = Wert der Betriebsart; Index 0 (Auto) bleibt ungenutzt.
        constexpr ModeParams kDefaultParams[kModeCount + 1] = {
            {0, 0}, // Auto - nie aktiv
            {1, 4}, // Komfort
            {1, 2}, // Standby
            {1, 1}, // Eco / Nacht - der Deckel ist der Zweck des Projekts
            {0, 1}, // Frost-/Gebaeudeschutz
            {4, 4}, // Stosslueften
            {1, 2}, // Temperatur-Absenkung
            {0, 0}  // Ruhe (Aus)
        };

        uint8_t modeIndex(OperatingMode mode)
        {
            const uint8_t i = static_cast<uint8_t>(mode);
            return i > kModeCount ? 0 : i;
        }
    } // namespace

    StageArbiter::StageArbiter()
    {
        for (uint8_t i = 0; i <= kModeCount; i++)
            mParams[i] = kDefaultParams[i];

        mResult = {0, StageSource::Automatic, false, Direction::Supply,
                   OperatingMode::Standby, 11};
        evaluateMode();
        evaluateStage();
    }

    uint8_t StageArbiter::clampStage(uint8_t stage)
    {
        return stage > kStageMax ? kStageMax : stage;
    }

    bool StageArbiter::expired(uint32_t now, uint32_t since, uint32_t runtime)
    {
        // runtime 0 heisst unendlich. Die Subtraktion ist ueberlaufsicher: bei einem
        // Ueberlauf von millis() bleibt die Differenz richtig.
        return runtime != 0 && (now - since) >= runtime;
    }

    // ------------------------------------------------------------ Parametrierung

    void StageArbiter::setModeParams(OperatingMode mode, ModeParams params)
    {
        const uint8_t i = modeIndex(mode);
        if (i == 0)
            return;
        params.baseStage = clampStage(params.baseStage);
        params.maxStage = clampStage(params.maxStage);
        mParams[i] = params;
    }

    ModeParams StageArbiter::modeParams(OperatingMode mode) const
    {
        return mParams[modeIndex(mode)];
    }

    void StageArbiter::setStandardMode(OperatingMode mode)
    {
        // "Auto" als Standard waere ein Verweis auf sich selbst.
        if (modeIndex(mode) != 0)
            mStandardMode = mode;
    }

    void StageArbiter::setNightMode(OperatingMode mode)
    {
        if (modeIndex(mode) != 0)
            mNightMode = mode;
    }

    void StageArbiter::setProtectionStage(uint8_t stage)
    {
        mProtectionStage = clampStage(stage);
    }

    void StageArbiter::setForcedObject(uint8_t index, OperatingMode mode,
                                       uint32_t runtimeMs)
    {
        if (index < 1 || index > 3 || modeIndex(mode) == 0)
            return;
        mForcedObjectMode[index] = mode;
        mForcedObjectRuntime[index] = runtimeMs;
    }

    void StageArbiter::setForcedObjectHierarchical(bool hierarchical)
    {
        mForcedObjectHierarchical = hierarchical;
    }

    void StageArbiter::setForcedModeRuntime(uint32_t runtimeMs)
    {
        mForcedModeRuntime = runtimeMs;
    }

    void StageArbiter::setManualRuntime(uint32_t runtimeMs)
    {
        mManualRuntime = runtimeMs;
    }

    // ------------------------------------------------------------ Raenge 1 bis 4

    void StageArbiter::setLock(bool active) { mLock = active; }
    void StageArbiter::setProtection(bool active) { mProtection = active; }

    void StageArbiter::setAirDemand(bool active, Direction direction, uint8_t stage)
    {
        mAirDemand = active;
        mAirDemandDirection = direction;
        mAirDemandStage = clampStage(stage);
    }

    void StageArbiter::setDirectionOverride(bool active, Direction direction)
    {
        mDirectionOverride = active;
        mOverrideDirection = direction;
    }

    void StageArbiter::setAutomaticSuppressed(bool suppressed)
    {
        mAutomaticSuppressed = suppressed;
    }

    void StageArbiter::setGroupStage(bool active, uint8_t stage, Direction direction)
    {
        mGroup = active;
        mGroupStage = clampStage(stage);
        mGroupDirection = direction;
    }

    // ------------------------------------------------------------ Rang 5

    void StageArbiter::setManualStage(uint8_t stage, uint32_t now)
    {
        mManualStage = clampStage(stage);
        mManualActive = true;
        mManualSince = now;
        resetAbove(5); // Rang 5 ist der hoechste ruecksetzbare - es bleibt nichts
    }

    void StageArbiter::stepManual(int8_t delta, uint32_t now)
    {
        // Ein Taster arbeitet auf dem, was gerade zu sehen ist - nicht auf einem
        // Handwert, der vor einer Stunde galt.
        int16_t base = mManualActive ? mManualStage : mResult.stage;
        base += delta;
        if (base < 0)
            base = 0;
        if (base > kStageMax)
            base = kStageMax;
        setManualStage(static_cast<uint8_t>(base), now);
    }

    void StageArbiter::setManualActive(bool active, uint32_t now)
    {
        if (active)
        {
            mManualActive = true;
            mManualSince = now;
        }
        else
        {
            mManualActive = false;
        }
    }

    // ------------------------------------------------------------ Rang 6

    void StageArbiter::setGuidanceStage(uint8_t stage)
    {
        mGuidanceStage = clampStage(stage);
    }

    // ------------------------------------------------------------ Raenge 7 bis 11

    void StageArbiter::setForcedObjectState(uint8_t index, bool active, uint32_t now)
    {
        if (index < 1 || index > 3)
            return;
        if (mForcedObjectOn[index] == active)
            return; // keine Aenderung, kein Ruecksetzen

        mForcedObjectOn[index] = active;
        if (active)
        {
            mForcedObjectSince[index] = now;
            mForcedObjectLast = index;
        }
        else if (mForcedObjectLast == index)
        {
            // Faellt das zuletzt eingeschaltete weg, uebernimmt ein noch
            // anstehendes - das hoechste noch aktive gilt als das juengste.
            mForcedObjectLast = 0;
            for (uint8_t i = 1; i <= 3; i++)
                if (mForcedObjectOn[i])
                    mForcedObjectLast = i;
        }
        resetAbove(7);
    }

    void StageArbiter::setNight(bool active, uint32_t now)
    {
        (void)now;
        if (mNight == active)
            return;
        mNight = active;
        resetAbove(8);
    }

    void StageArbiter::setForcedMode(OperatingMode mode, uint32_t now)
    {
        if (modeIndex(mode) == 0)
        {
            clearForcedMode(now);
            return;
        }
        mForcedModeOn = true;
        mForcedMode = mode;
        mForcedModeSince = now;
        resetAbove(9);
    }

    void StageArbiter::clearForcedMode(uint32_t now)
    {
        (void)now;
        if (!mForcedModeOn)
            return;
        mForcedModeOn = false;
        resetAbove(9);
    }

    void StageArbiter::setMode(OperatingMode mode, uint32_t now)
    {
        (void)now;
        if (mMode == mode)
            return;
        mMode = mode;
        resetAbove(10);
    }

    // ------------------------------------------------------------ Ruecksetzen

    void StageArbiter::resetAbove(uint8_t rank)
    {
        // Alle Objekte mit hoeherem Vorrang (kleinere Zahl) bis einschliesslich 5.
        // Raenge 1 bis 4 sind ausgenommen: eine Sperre oder ein Schutz laesst sich
        // nicht durch Bedienung wegdruecken.
        if (rank > 9)
        {
            mForcedModeOn = false;
        }
        if (rank > 8)
        {
            mNight = false;
        }
        if (rank > 7)
        {
            for (uint8_t i = 1; i <= 3; i++)
                mForcedObjectOn[i] = false;
            mForcedObjectLast = 0;
        }
        if (rank > 5)
        {
            mManualActive = false;
        }
    }

    // ------------------------------------------------------------ Auswertung

    void StageArbiter::expireRuntimes(uint32_t now)
    {
        if (mManualActive && expired(now, mManualSince, mManualRuntime))
            mManualActive = false;

        if (mForcedModeOn && expired(now, mForcedModeSince, mForcedModeRuntime))
            mForcedModeOn = false;

        for (uint8_t i = 1; i <= 3; i++)
        {
            if (mForcedObjectOn[i] &&
                expired(now, mForcedObjectSince[i], mForcedObjectRuntime[i]))
            {
                mForcedObjectOn[i] = false;
                if (mForcedObjectLast == i)
                {
                    mForcedObjectLast = 0;
                    for (uint8_t k = 1; k <= 3; k++)
                        if (mForcedObjectOn[k])
                            mForcedObjectLast = k;
                }
            }
        }
    }

    void StageArbiter::evaluateMode()
    {
        // Rang 7: Zwangsobjekte.
        uint8_t chosen = 0;
        if (mForcedObjectHierarchical)
        {
            // "hierarchisch - 1 vor 2 vor 3", so wie es in der ETS steht: das
            // Zwangsobjekt mit der KLEINSTEN Nummer gewinnt.
            for (uint8_t i = 3; i >= 1; i--)
                if (mForcedObjectOn[i])
                    chosen = i;
        }
        else if (mForcedObjectLast != 0 && mForcedObjectOn[mForcedObjectLast])
        {
            chosen = mForcedObjectLast;
        }

        if (chosen != 0)
        {
            mActiveMode = mForcedObjectMode[chosen];
            mActiveModeRank = 7;
            return;
        }

        if (mNight) // Rang 8
        {
            mActiveMode = mNightMode;
            mActiveModeRank = 8;
            return;
        }

        if (mForcedModeOn) // Rang 9
        {
            mActiveMode = mForcedMode;
            mActiveModeRank = 9;
            return;
        }

        if (modeIndex(mMode) != 0) // Rang 10
        {
            mActiveMode = mMode;
            mActiveModeRank = 10;
            return;
        }

        mActiveMode = mStandardMode; // Rang 11
        mActiveModeRank = 11;
    }

    void StageArbiter::evaluateStage()
    {
        const ModeParams p = mParams[modeIndex(mActiveMode)];

        mResult.mode = mActiveMode;
        mResult.modeRank = mActiveModeRank;
        // Die reine Richtungsvorgabe (Rang 3) steht vorn, damit sie fuer alles
        // gilt, was aus Rang 4 oder tiefer kommt - Verbund, Handstufe, Automatik.
        // Sie sagt nur, wohin gefoerdert wird, nicht wie viel.
        mResult.directionForced = mDirectionOverride;
        mResult.direction = mDirectionOverride ? mOverrideDirection : Direction::Supply;

        // Rang 1: Sperre. Stufe 0 heisst in KwlCurve der sichere Zustand, also
        // 5,00 V beim bipolaren Kanal - nicht Code 0.
        if (mLock)
        {
            mResult.stage = 0;
            mResult.source = StageSource::Lock;
            // Stillstand hat keine Richtung.
            mResult.directionForced = false;
            mResult.direction = Direction::Supply;
            return;
        }

        // Rang 2: Schutz. Abweichung von Arcus - ein durchfrierender Raum ist kein
        // Bedienfall, deshalb steht der Schutz ueber Hand und Verbund.
        if (mProtection)
        {
            mResult.stage = mProtectionStage;
            mResult.source = StageSource::Protection;
            return;
        }

        // Rang 3: Zu-/Abluftanforderung erzwingt zusaetzlich die Richtung.
        if (mAirDemand)
        {
            mResult.stage = mAirDemandStage;
            mResult.source = StageSource::AirDemand;
            mResult.directionForced = true;
            mResult.direction = mAirDemandDirection;
            return;
        }

        // Rang 4: Verbund. Nur beim Slave gesetzt. Eine Richtungsvorgabe aus
        // Rang 3 steht darueber und bleibt stehen.
        if (mGroup)
        {
            mResult.stage = mGroupStage;
            mResult.source = StageSource::Group;
            if (!mDirectionOverride)
            {
                mResult.directionForced = true;
                mResult.direction = mGroupDirection;
            }
            return;
        }

        // Rang 5: Handstufe. Bewusst NICHT von der Maximalstufe gedeckelt - der
        // Deckel schuetzt vor der Automatik, nicht vor dem Bewohner.
        if (mManualActive)
        {
            mResult.stage = mManualStage;
            mResult.source = StageSource::Manual;
            return;
        }

        // Rang 6: Grundstufe laeuft immer, Fuehrungen heben an, Maximalstufe
        // deckelt. Eine Grundstufe ueber der Maximalstufe ist eine Fehlparametrierung
        // und wird gedeckelt, nicht durchgelassen.
        uint8_t stage = 0;
        if (!mAutomaticSuppressed)
        {
            stage = p.baseStage > mGuidanceStage ? p.baseStage : mGuidanceStage;
            if (stage > p.maxStage)
                stage = p.maxStage;
        }
        mResult.stage = stage;
        mResult.source = StageSource::Automatic;
    }

    StageResult StageArbiter::update(uint32_t now)
    {
        expireRuntimes(now);
        evaluateMode();
        evaluateStage();
        return mResult;
    }

    void StageArbiter::reset()
    {
        mLock = false;
        mProtection = false;
        mAirDemand = false;
        mDirectionOverride = false;
        mAutomaticSuppressed = false;
        mGroup = false;
        mManualActive = false;
        mManualStage = 0;
        mGuidanceStage = 0;
        for (uint8_t i = 1; i <= 3; i++)
            mForcedObjectOn[i] = false;
        mForcedObjectLast = 0;
        mNight = false;
        mForcedModeOn = false;
        mMode = OperatingMode::Auto;
        evaluateMode();
        evaluateStage();
    }
} // namespace Kwl
