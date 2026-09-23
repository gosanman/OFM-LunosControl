#include "CableComp.h"

namespace Kwl
{
    float CableComp::resistance(float lengthMeter, float sectionMm2)
    {
        if (!(lengthMeter > 0.0f) || !(sectionMm2 > 0.0f))
            return 0.0f;
        return kRhoCopper * lengthMeter / sectionMm2;
    }

    float CableComp::deltaVolt(float currentAmp, float resistanceOhm)
    {
        if (!(currentAmp > 0.0f) || !(resistanceOhm > 0.0f))
            return 0.0f;
        return currentAmp * resistanceOhm;
    }

    float CableComp::apply(float nominalVolt, float deltaVolt)
    {
        float v = nominalVolt;
        if (deltaVolt > 0.0f)
            v += deltaVolt;
        if (!(v > 0.0f))
            return 0.0f;
        if (v > kVoltMax)
            return kVoltMax;
        return v;
    }
} // namespace Kwl
