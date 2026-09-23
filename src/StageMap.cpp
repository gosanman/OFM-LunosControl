#include "StageMap.h"

namespace Kwl
{
    namespace
    {
        /// Untere Grenze der Stufen 1…4 ohne Hysterese.
        constexpr uint8_t kEdge[kStageMax + 1] = {0, 1, 26, 51, 76};

        /// Breite der Hysterese in Prozentpunkten.
        constexpr uint8_t kHyst = 3;

        /// Mitte des Bereichs jeder Stufe, fuer die Rueckmeldung.
        constexpr uint8_t kCenter[kStageMax + 1] = {0, 13, 38, 63, 88};
    } // namespace

    void StageMap::setStage(uint8_t stage)
    {
        mStage = stage > kStageMax ? kStageMax : stage;
    }

    uint8_t StageMap::stageToPercent(uint8_t stage)
    {
        return kCenter[stage > kStageMax ? kStageMax : stage];
    }

    uint8_t StageMap::update(uint8_t percent)
    {
        const uint8_t p = percent > 100 ? 100 : percent;

        // Null ist kein Randwert, sondern eine Ansage.
        if (p == 0)
        {
            mStage = 0;
            return mStage;
        }

        // Hoch: eine Stufe wird erst kHyst ueber ihrer Grenze betreten.
        while (mStage < kStageMax && p >= kEdge[mStage + 1] + kHyst)
            mStage++;

        // Runter: und erst kHyst unter ihrer Grenze wieder verlassen. Stufe 1 ist
        // dabei der Boden - darunter kommt man nur ueber die Null.
        while (mStage > 1 && p + kHyst <= kEdge[mStage])
            mStage--;

        // Jeder Wert ab 1 % bedeutet mindestens Stufe 1.
        if (mStage == 0)
            mStage = 1;

        return mStage;
    }
} // namespace Kwl
