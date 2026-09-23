#include "KwlRoom.h"

#include "knxprod.h"

namespace Kwl
{
    KwlRoom::KwlRoom(uint8_t index)
    {
        _channelIndex = index;
    }

    void KwlRoom::setup()
    {
        mActive = ParamROOM_rActive;
        if (!mActive)
            return;

        // Grenzwerte der Feuchtetreppe. Die uebrigen Fuehrungen und der
        // Parametersatz je Betriebsart folgen in Woche 3 - die Rechenschichten
        // dahinter sind bereits geprueft.
        const float humidity[kStageMax + 1] = {
            static_cast<float>(ParamROOM_rHumGW0), static_cast<float>(ParamROOM_rHumGW1),
            static_cast<float>(ParamROOM_rHumGW2), static_cast<float>(ParamROOM_rHumGW3),
            static_cast<float>(ParamROOM_rHumGW4)};
        mHumidity.setThresholds(humidity);

        mCompare.setThresholds(static_cast<float>(ParamROOM_rDehumOn) / 10.0f,
                               static_cast<float>(ParamROOM_rDehumOff) / 10.0f);

        mTemp.setFrostLimit(static_cast<float>(ParamROOM_rFrostTemp));
        mTemp.setHeatLimit(static_cast<float>(ParamROOM_rHeatTemp));
        mTemp.setStages(ParamROOM_rCoolStage, ParamROOM_rHeatStage);

        mArbiter.setStandardMode(static_cast<OperatingMode>(ParamROOM_rDefaultMode));
    }

    void KwlRoom::loop()
    {
        if (!mActive)
            return;

        // Woche 3: Sensorwerte bewerten, Fuehrungen rechnen, Ergebnis in den
        // Arbiter geben. Bis dahin liefert er die Grundstufe der Betriebsart.
        mArbiter.update(millis());
    }

    void KwlRoom::processInputKo(GroupObject& ko)
    {
        (void)ko;
        // Woche 3: Sensor-KOs, Betriebsart, Zwangsobjekte, Handstufe.
    }

    void KwlRoom::printStatusLine()
    {
        if (!mActive)
        {
            logInfoP("Raum %u: nicht aktiv", (unsigned)(_channelIndex + 1));
            return;
        }

        const StageResult r = mArbiter.result();
        logInfoP("Raum %u: Stufe %u aus Rang %u, Betriebsart %u von Ebene %u",
                 (unsigned)(_channelIndex + 1), (unsigned)r.stage,
                 (unsigned)static_cast<uint8_t>(r.source),
                 (unsigned)static_cast<uint8_t>(r.mode), (unsigned)r.modeRank);
    }
} // namespace Kwl
