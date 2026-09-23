#include "TempControl.h"

namespace Kwl
{
    namespace
    {
        /// Rueckkehr aus dem Frostschutz erst 2 K ueber dem Grenzwert (Arcus 3.10).
        constexpr float kFrostReturn = 2.0f;

        /// Das Kelvin aus der Tabelle: Einschaltabstand der freien Kuehlung ueber
        /// Tsoll und der Warmluftnutzung darunter.
        constexpr float kBand = 1.0f;

        bool isNan(float v) { return !(v == v); }
    } // namespace

    void TempControl::setFrostLimit(float celsius) { mFrostLimit = celsius; }
    void TempControl::setHeatLimit(float celsius) { mHeatLimit = celsius; }

    void TempControl::setDistance(float kelvin)
    {
        // Ein negativer Abstand wuerde die Bedingung umdrehen: die Fuehrung wuerde
        // dann eingreifen, WEIL draussen aehnlich warm ist.
        mDistance = kelvin < 0.0f ? 0.0f : kelvin;
    }

    void TempControl::setStages(uint8_t cooling, uint8_t heating)
    {
        mStageCooling = cooling > kStageMax ? kStageMax : cooling;
        mStageHeating = heating > kStageMax ? kStageMax : heating;
    }

    void TempControl::reset()
    {
        mFrost = false;
        mHeat = false;
        mMode = TempMode::None;
    }

    TempResult TempControl::result() const
    {
        TempResult r;
        r.frostProtection = mFrost;
        r.heatProtection = mHeat;
        r.mode = mMode;

        switch (mMode)
        {
            case TempMode::FreeCooling:
                r.stageRequest = mStageCooling;
                r.cycle = CycleRule::Summer;
                break;
            case TempMode::UseWarmAir:
                r.stageRequest = mStageHeating;
                r.cycle = CycleRule::Summer;
                break;
            case TempMode::HeatRetention:
                // Waerme halten heisst nicht foerdern, sondern kurz takten: die
                // Anforderung bleibt leer, nur der Takt wird vorgegeben.
                r.stageRequest = 0;
                r.cycle = CycleRule::Wrg;
                break;
            case TempMode::None:
            default:
                r.stageRequest = 0;
                r.cycle = CycleRule::FromOperatingMode;
                break;
        }
        return r;
    }

    TempResult TempControl::update(float tempInside, float tempOutside,
                                   float tempSetpoint)
    {
        // Ein ausgefallener Sensor darf weder Schutz ausloesen noch einen laufenden
        // Schutz aufheben. Der Ausfall selbst wird eine Ebene hoeher gemeldet.
        if (isNan(tempInside) || isNan(tempOutside) || isNan(tempSetpoint))
            return result();

        // --- Schutz zuerst -------------------------------------------------
        // Frostschutz mit Rueckkehr bei +2 K. Ohne diesen Abstand wuerde der Knoten
        // am Grenzwert im Takt der Messwerte zwischen "aus" und "an" springen.
        if (tempInside < mFrostLimit)
            mFrost = true;
        else if (tempInside >= mFrostLimit + kFrostReturn)
            mFrost = false;

        // Hitzeschutz: drinnen zu warm UND draussen noch waermer - Lueften wuerde
        // die Waerme hereinholen. Der Plan nennt hier keine Hysterese; die zweite
        // Bedingung wirkt als eine.
        mHeat = tempInside > mHeatLimit && tempOutside > tempInside;

        // Schutz schlaegt Fuehrung: solange er ansteht, gibt es keinen Eingriff aus
        // der Temperaturfuehrung, und der Zustand faengt danach neu an.
        if (mFrost || mHeat)
        {
            mMode = TempMode::None;
            return result();
        }

        // --- Fuehrung ------------------------------------------------------
        const bool outsideCooler = tempOutside < tempInside - mDistance;
        const bool outsideWarmer = tempOutside > tempInside + mDistance;

        if (outsideCooler)
        {
            if (tempInside > tempSetpoint + kBand)
                mMode = TempMode::FreeCooling;
            else if (tempInside < tempSetpoint)
                mMode = TempMode::HeatRetention;
            else if (mMode != TempMode::FreeCooling && mMode != TempMode::HeatRetention)
                mMode = TempMode::None; // im Band, aber ohne haltbaren Vorzustand
            // sonst: im Band, letzter Zustand bleibt stehen
        }
        else if (outsideWarmer)
        {
            if (tempInside < tempSetpoint - kBand)
                mMode = TempMode::UseWarmAir;
            else if (tempInside >= tempSetpoint)
                mMode = TempMode::None;
            else if (mMode != TempMode::UseWarmAir)
                mMode = TempMode::None; // im Band, aber ohne haltbaren Vorzustand
            // sonst: im Band, Warmluftnutzung bleibt stehen
        }
        else
        {
            // Innen und aussen liegen zu dicht beieinander: die Fuehrung haelt sich
            // heraus, damit sie nicht gegen die Heizungsregelung arbeitet.
            mMode = TempMode::None;
        }

        return result();
    }
} // namespace Kwl
