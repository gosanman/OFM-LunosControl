#include "StageLadder.h"

namespace Kwl
{
    void StageLadder::setThresholds(const float thresholds[kStageMax + 1])
    {
        for (uint8_t i = 0; i <= kStageMax; i++)
        {
            float v = thresholds[i];
            // Aufsteigend zurechtruecken. Eine fallende Reihe wuerde bedeuten, dass
            // derselbe Messwert eine Stufe gleichzeitig ein- und ausschaltet.
            if (i > 0 && v < mThreshold[i - 1])
                v = mThreshold[i - 1];
            mThreshold[i] = v;
        }
    }

    float StageLadder::threshold(uint8_t index) const
    {
        return mThreshold[index > kStageMax ? kStageMax : index];
    }

    uint8_t StageLadder::update(float value)
    {
        // NaN darf die Stufe nicht veraendern: ein ausgefallener Sensor ist kein
        // Grund, die Lueftung hoch- oder herunterzufahren. Der Ausfall selbst wird
        // eine Ebene hoeher behandelt (Sensorueberwachung, Fehlercode).
        if (!(value == value))
            return mStage;

        // Hoch, solange der Einschaltgrenzwert der naechsten Stufe erreicht ist.
        while (mStage < kStageMax && value >= mThreshold[mStage + 1])
            mStage++;

        // Runter, solange der Ausschaltgrenzwert der aktuellen Stufe unterschritten
        // ist. Der Ausschaltgrenzwert von Stufe n ist GW(n-1) - daher der Abstand
        // von einer ganzen Stufe, in dem sich nichts aendert.
        while (mStage > 0 && value < mThreshold[mStage - 1])
            mStage--;

        return mStage;
    }
} // namespace Kwl
