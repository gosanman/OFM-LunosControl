#include "MoistAir.h"

#include <cmath>

namespace Kwl
{
    namespace
    {
        // Gueltigkeitsbereich der Magnus-Naeherung ueber Wasser.
        constexpr float kTempMin = -45.0f;
        constexpr float kTempMax = 60.0f;

        // Bereich des ETS-Parameters "Hoehe ueber Meer".
        constexpr float kAltitudeMax = 2000.0f;

        float clampF(float v, float lo, float hi)
        {
            if (!(v == v)) // NaN
                return lo;
            if (v < lo)
                return lo;
            if (v > hi)
                return hi;
            return v;
        }
    } // namespace

    float MoistAir::saturationPressure(float tempC)
    {
        const float t = clampF(tempC, kTempMin, kTempMax);
        return 6.112f * std::exp(17.62f * t / (243.12f + t));
    }

    float MoistAir::vapourPressure(float tempC, float relHumidity)
    {
        const float rh = clampF(relHumidity, 0.0f, 100.0f);
        return rh / 100.0f * saturationPressure(tempC);
    }

    float MoistAir::pressureAtAltitude(float altitudeMeter)
    {
        const float h = clampF(altitudeMeter, 0.0f, kAltitudeMax);
        return 1013.25f * std::pow(1.0f - 2.25577e-5f * h, 5.2559f);
    }

    float MoistAir::mixingRatio(float vapourPressureHpa, float airPressureHpa)
    {
        // Ohne Luftdruck gibt es kein Mischungsverhaeltnis. Diese Pruefung steht
        // VOR der Klemmung: bei p <= 1 hPa liegt die obere Klemmgrenze unter der
        // unteren, und clampF liefert dann die obere - ein negatives Ergebnis.
        if (!(airPressureHpa > 1.0f))
            return 0.0f;

        // Ein unsinniger Partialdruck oberhalb des Luftdrucks wuerde sonst durch
        // eine Division nahe null laufen.
        const float e = clampF(vapourPressureHpa, 0.0f, airPressureHpa - 1.0f);
        return 622.0f * e / (airPressureHpa - e);
    }

    float MoistAir::mixingRatioAt(float tempC, float relHumidity, float altitudeMeter)
    {
        return mixingRatio(vapourPressure(tempC, relHumidity),
                           pressureAtAltitude(altitudeMeter));
    }

    void HumidityCompare::setThresholds(float onHpa, float offHpa)
    {
        // Der Ausschaltwert muss unter dem Einschaltwert liegen, sonst gibt es keine
        // Hysterese, sondern eine Schwelle mit unklarer Richtung.
        mOnHpa = onHpa;
        mOffHpa = offHpa < onHpa ? offHpa : onHpa;
    }

    bool HumidityCompare::update(float vapourInside, float vapourOutside)
    {
        const float delta = vapourInside - vapourOutside;

        // Ein fehlender Sensorwert darf die Sperre nicht aufheben, aber auch keine
        // laufende Entfeuchtung abbrechen: der Zustand bleibt stehen.
        if (!(delta == delta))
            return mAllowed;

        if (delta >= mOnHpa)
            mAllowed = true;
        else if (delta <= mOffHpa)
            mAllowed = false;

        return mAllowed;
    }
} // namespace Kwl
