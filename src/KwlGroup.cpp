#include "KwlGroup.h"

namespace Kwl
{
    KwlGroup::KwlGroup()
    {
        mResult = {0, false, Direction::Supply, false, false, 0, 70, true, 1.0f};
    }

    Direction KwlGroup::opposite(Direction d)
    {
        return d == Direction::Supply ? Direction::Exhaust : Direction::Supply;
    }

    Direction KwlGroup::implied(const GroupMember& m)
    {
        // Ein Mitglied in Phase 1 laeuft gegenlaeufig zur Verbundrichtung. Seine
        // Forderung bedeutet also die Gegenrichtung fuer Phase 0.
        return m.phase == 0 ? m.direction : opposite(m.direction);
    }

    Direction KwlGroup::directionFor(const GroupResult& r, uint8_t phase)
    {
        return phase == 0 ? r.phase0Direction : opposite(r.phase0Direction);
    }

    uint8_t KwlGroup::stageForShare(uint8_t groupStage, uint8_t share)
    {
        if (groupStage == 0)
            return 0;
        if (share >= 100)
            return groupStage > kStageMax ? kStageMax : groupStage;

        // Kaufmaennisch runden, ohne Gleitkomma.
        const uint16_t scaled = (static_cast<uint16_t>(groupStage) * share + 50) / 100;
        if (scaled == 0)
            return 1; // ein Anteil darf einen laufenden Luefter nicht anhalten
        return scaled > kStageMax ? kStageMax : static_cast<uint8_t>(scaled);
    }

    // ------------------------------------------------------------ Parametrierung

    void KwlGroup::setStageRule(GroupStageRule rule) { mStageRule = rule; }
    void KwlGroup::setCycleConflict(GroupCycleConflict rule) { mCycleConflict = rule; }

    void KwlGroup::setLeadRoom(uint8_t roomNo) { mLeadRoom = roomNo; }

    void KwlGroup::setDeadTime(uint16_t seconds) { mDeadTime = seconds; }

    void KwlGroup::setCycleTime(uint8_t stage, uint16_t seconds)
    {
        if (stage < 1 || stage > kStageMax || seconds == 0)
            return;
        mCycleTime[stage] = seconds;
    }

    void KwlGroup::setSummerCycleTime(uint16_t seconds)
    {
        if (seconds != 0)
            mSummerCycle = seconds;
    }

    // ------------------------------------------------------------ Mitglieder

    void KwlGroup::clearMembers() { mCount = 0; }

    bool KwlGroup::addMember(const GroupMember& member)
    {
        if (mCount >= kGroupMembersMax)
            return false;
        mMember[mCount] = member;
        if (mMember[mCount].stage > kStageMax)
            mMember[mCount].stage = kStageMax;
        if (mMember[mCount].maxStage > kStageMax)
            mMember[mCount].maxStage = kStageMax;
        mCount++;
        return true;
    }

    // ------------------------------------------------------------ Stufenregel

    uint8_t KwlGroup::computeStage() const
    {
        if (mCount == 0)
            return 0;

        switch (mStageRule)
        {
            case GroupStageRule::Min:
            {
                uint8_t lowest = kStageMax;
                for (uint8_t i = 0; i < mCount; i++)
                    if (mMember[i].stage < lowest)
                        lowest = mMember[i].stage;
                return lowest;
            }

            case GroupStageRule::LeadRoom:
            {
                for (uint8_t i = 0; i < mCount; i++)
                    if (mMember[i].roomNo == mLeadRoom)
                        return mMember[i].stage;
                // Der fuehrende Raum ist nicht im Verbund - das ist eine
                // Fehlparametrierung. Dann gilt die sichere Regel: das Minimum.
                uint8_t lowest = kStageMax;
                for (uint8_t i = 0; i < mCount; i++)
                    if (mMember[i].stage < lowest)
                        lowest = mMember[i].stage;
                return lowest;
            }

            case GroupStageRule::MaxCapped:
            default:
            {
                // Der hoechste Wunsch zaehlt, aber der kleinste Deckel gilt: der
                // Nachtdeckel im Schlafzimmer darf nicht dadurch fallen, dass im
                // Nachbarraum das CO2 steigt.
                uint8_t highest = 0;
                uint8_t cap = kStageMax;
                for (uint8_t i = 0; i < mCount; i++)
                {
                    if (mMember[i].stage > highest)
                        highest = mMember[i].stage;
                    if (mMember[i].maxStage < cap)
                        cap = mMember[i].maxStage;
                }
                return highest > cap ? cap : highest;
            }
        }
    }

    // ------------------------------------------------------------ Zykluszeit

    uint16_t KwlGroup::computeCycleSeconds(uint8_t stage)
    {
        const uint8_t s = stage == 0 ? 1 : stage;
        mSummerChosen = false;

        if (mCycleConflict == GroupCycleConflict::LeadRoomWins)
        {
            for (uint8_t i = 0; i < mCount; i++)
                if (mMember[i].roomNo == mLeadRoom)
                {
                    mSummerChosen = mMember[i].cycleWish == CycleRule::Summer;
                    return mSummerChosen ? mSummerCycle : mCycleTime[s];
                }
        }

        // Kuerzester Zyklus gewinnt: wer WRG will, bekommt sie. Ein langer Zyklus
        // ist die Abwesenheit von WRG - die laesst sich nicht erzwingen, wenn der
        // Nachbar sie braucht.
        uint16_t shortest = 0;
        bool summer = true;
        for (uint8_t i = 0; i < mCount; i++)
        {
            const bool wantsSummer = mMember[i].cycleWish == CycleRule::Summer;
            const uint16_t wish = wantsSummer ? mSummerCycle : mCycleTime[s];
            if (shortest == 0 || wish < shortest)
            {
                shortest = wish;
                summer = wantsSummer;
            }
        }
        mSummerChosen = summer;
        return shortest == 0 ? mCycleTime[s] : shortest;
    }

    // ------------------------------------------------------------ Richtung

    void KwlGroup::resolveDemands(uint8_t stage)
    {
        (void)stage;
        mFixed = false;
        mConflict = false;
        mConflictRoom = 0;

        int8_t winner = -1;
        for (uint8_t i = 0; i < mCount; i++)
        {
            if (!mMember[i].directionDemanded)
                continue;
            if (winner < 0)
            {
                winner = static_cast<int8_t>(i);
                continue;
            }
            // Die hoehere Stufe fuehrt; bei Gleichstand die niedrigere Raumnummer.
            const GroupMember& w = mMember[winner];
            const GroupMember& c = mMember[i];
            if (c.stage > w.stage || (c.stage == w.stage && c.roomNo < w.roomNo))
                winner = static_cast<int8_t>(i);
        }

        if (winner < 0)
            return; // keine Forderung, der Takt laeuft frei

        mFixed = true;
        mDemandDirection = implied(mMember[winner]);

        // Wer eine andere Verbundrichtung verlangt hat, ist unterlegen. Gemeldet
        // wird der kleinste betroffene Raum; die uebrigen stehen im Status.
        for (uint8_t i = 0; i < mCount; i++)
        {
            if (!mMember[i].directionDemanded)
                continue;
            if (implied(mMember[i]) == mDemandDirection)
                continue;
            mConflict = true;
            if (mConflictRoom == 0 || mMember[i].roomNo < mConflictRoom)
                mConflictRoom = mMember[i].roomNo;
        }
    }

    // ------------------------------------------------------------ Takt

    GroupResult KwlGroup::update(uint32_t now)
    {
        // Der erste Aufruf setzt den Zeitbezug. Ohne das wuerde ein Verbund, dessen
        // erster update() spaet nach dem Start kommt, sofort einen Wechsel sehen -
        // die Differenz zu einem nie gesetzten Startzeitpunkt ist beliebig gross.
        if (!mStarted)
        {
            mStarted = true;
            mSince = now;
            mCycleSince = now;
        }

        const uint8_t stage = computeStage();
        const uint16_t cycle = computeCycleSeconds(stage);
        resolveDemands(stage);

        mResult.stage = stage;
        mResult.cycleSeconds = cycle;
        // WRG heisst: es wird gependelt, und zwar kurz. Eine feste Richtung ist
        // keine Waermerueckgewinnung, ein Stundenzyklus praktisch auch nicht.
        mResult.hrv = stage > 0 && !mFixed && !mSummerChosen;
        mResult.directionFixed = mFixed;
        mResult.conflict = mConflict;
        mResult.conflictRoom = mConflictRoom;

        // Stufe 0 ist Stillstand: nichts dreht sich, also gibt es auch nichts zu
        // wechseln. Der Takt faengt beim naechsten Anlauf von vorn an, damit die
        // erste Richtung eine volle Zykluszeit steht. Eine laufende Rampe oder
        // Totzeit ist damit erledigt.
        if (stage == 0)
        {
            mPhase = Phase::Run;
            mInDeadTime = false;
            mWasRunning = false;
            mSince = now;
            mCycleSince = now;
            mResult.inDeadTime = false;
            mResult.phase0Direction = mCurrent;
            mResult.ramp = 1.0f;
            return mResult;
        }

        const uint32_t rampMs = static_cast<uint32_t>(mRampTime) * 1000u;
        const uint32_t deadMs = static_cast<uint32_t>(mDeadTime) * 1000u;

        switch (mPhase)
        {
            case Phase::Run:
            {
                // Anlauf aus dem Stand: die erste Richtung steht eine volle
                // Zykluszeit.
                if (!mWasRunning)
                    mCycleSince = now;

                const Direction t = target(now, cycle);
                if (t != mCurrent)
                {
                    // Sicherheitsinvariante 5: zwischen zwei Richtungen liegt
                    // immer ein Aufenthalt bei 5,00 V. Mit Rampe zuerst hinunter -
                    // ausser der Verbund stand, dann gibt es nichts abzubremsen.
                    mPending = t;
                    mSince = now;
                    mPhase = (rampMs > 0 && mWasRunning) ? Phase::RampDown : Phase::Dead;
                }
                break;
            }

            case Phase::RampDown:
            {
                const Direction t = target(now, cycle);
                if (t == mCurrent)
                {
                    // Die Forderung ist weg, bevor 5,00 V erreicht sind: ohne
                    // Sprung und ohne Totzeit wieder hinauf - die Richtung hat
                    // nie gewechselt.
                    const float f = rampFactor(now);
                    mPhase = Phase::RampUp;
                    mSince = now - static_cast<uint32_t>(f * static_cast<float>(rampMs));
                }
                else
                {
                    mPending = t;
                    if ((now - mSince) >= rampMs)
                    {
                        // Unten: erst jetzt beginnt die Totzeit.
                        mPhase = Phase::Dead;
                        mSince = now;
                    }
                }
                break;
            }

            case Phase::Dead:
                // Waehrend der Totzeit wird das Ziel NICHT aus dem Takt neu
                // gebildet - der anstehende Wechsel wuerde sich sonst selbst
                // aufheben. Nur eine Forderung darf das Ziel noch verschieben; die
                // Totzeit laeuft dabei nicht neu an, sie ist ohnehin der sichere
                // Zustand.
                if (mFixed)
                    mPending = mDemandDirection;
                if ((now - mSince) >= deadMs)
                {
                    mCurrent = mPending;
                    mSince = now;
                    mCycleSince = now;
                    mPhase = rampMs > 0 ? Phase::RampUp : Phase::Run;
                }
                break;

            case Phase::RampUp:
                if (mFixed && mDemandDirection != mCurrent)
                {
                    // Neue Forderung in die Gegenrichtung, noch waehrend es
                    // hinaufgeht: von hier aus wieder hinunter, ohne Sprung.
                    const float f = rampFactor(now);
                    mPending = mDemandDirection;
                    mPhase = Phase::RampDown;
                    mSince = now - static_cast<uint32_t>((1.0f - f) * static_cast<float>(rampMs));
                }
                else if ((now - mSince) >= rampMs)
                {
                    mPhase = Phase::Run;
                }
                break;
        }

        mWasRunning = true;
        mInDeadTime = mPhase == Phase::Dead;
        mResult.inDeadTime = mInDeadTime;
        mResult.phase0Direction = mCurrent;
        mResult.ramp = rampFactor(now);
        return mResult;
    }

    Direction KwlGroup::target(uint32_t now, uint16_t cycle) const
    {
        if (mFixed)
            return mDemandDirection;
        if ((now - mCycleSince) >= static_cast<uint32_t>(cycle) * 1000u)
            return opposite(mCurrent);
        return mCurrent;
    }

    float KwlGroup::rampFactor(uint32_t now) const
    {
        const uint32_t rampMs = static_cast<uint32_t>(mRampTime) * 1000u;
        const uint32_t elapsed = now - mSince;
        switch (mPhase)
        {
            case Phase::Dead:
                return 0.0f;
            case Phase::RampDown:
                if (rampMs == 0 || elapsed >= rampMs)
                    return 0.0f;
                return 1.0f - static_cast<float>(elapsed) / static_cast<float>(rampMs);
            case Phase::RampUp:
                if (rampMs == 0 || elapsed >= rampMs)
                    return 1.0f;
                return static_cast<float>(elapsed) / static_cast<float>(rampMs);
            case Phase::Run:
            default:
                return 1.0f;
        }
    }

    void KwlGroup::reset()
    {
        mCount = 0;
        mFixed = false;
        mConflict = false;
        mConflictRoom = 0;
        mCurrent = Direction::Supply;
        mPending = Direction::Supply;
        mPhase = Phase::Run;
        mInDeadTime = false;
        mStarted = false;
        mWasRunning = false;
        mSince = 0;
        mCycleSince = 0;
        mResult = {0, false, Direction::Supply, false, false, 0, mCycleTime[1], true, 1.0f};
    }
} // namespace Kwl
