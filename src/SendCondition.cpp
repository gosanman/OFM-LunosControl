#include "SendCondition.h"

namespace Kwl
{
    bool SendCondition::passesDeadband(int32_t value, int32_t lastSent,
                                       uint8_t percentBand, uint32_t absBand)
    {
        const int32_t diff = (value > lastSent) ? (value - lastSent) : (lastSent - value);
        if (diff == 0)
            return false;

        // Relativ ODER absolut: der absolute Sockel wirkt dort, wo die relative
        // Schwelle in der Naehe von 0 beliebig klein wuerde.
        if (absBand > 0 && static_cast<uint32_t>(diff) >= absBand)
            return true;

        if (percentBand > 0)
        {
            const int32_t ref = (lastSent < 0) ? -lastSent : lastSent;
            // In 64 Bit rechnen: diff * 100 laeuft bei grossen Rohwerten sonst ueber.
            if (ref > 0 && static_cast<int64_t>(diff) * 100 >=
                               static_cast<int64_t>(ref) * percentBand)
                return true;
        }

        // Ohne jede Schwelle ist jede Aenderung sendenswert.
        return percentBand == 0 && absBand == 0;
    }

    bool SendCondition::cycleElapsed(uint32_t now, uint32_t lastSent, uint32_t cycleMs)
    {
        if (cycleMs == 0)
            return false;
        return (now - lastSent) >= cycleMs;
    }
} // namespace Kwl
