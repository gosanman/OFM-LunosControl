#include "KwlFan.h"

#include "knxprod.h"

namespace Kwl
{
    KwlFan::KwlFan(uint8_t index)
    {
        _channelIndex = index;
    }

    void KwlFan::setup()
    {
        // Die Param-Makros rechnen ueber _channelIndex; sie stehen deshalb erst
        // hier zur Verfuegung und nicht im Konstruktor.
        mActive = ParamFAN_fActive;
        if (!mActive)
            return;

        mType = static_cast<FanType>(ParamFAN_fType);
        mDacChannel = ParamFAN_fChannel;
        mRoomNo = ParamFAN_fRoom;
        mGroupNo = ParamFAN_fGroup;
        mPhase = ParamFAN_fPhase ? 1 : 0;
        mShare = ParamFAN_fShare;

        // Der ETS-Parameter zaehlt ab 1, die Ausgabeschicht ab 0.
        if (mDacChannel > 0)
            mDacChannel--;

        mStage = 0;
        mDirection = Direction::Supply;
    }

    void KwlFan::loop()
    {
        // Phase 4 der Firmware: Sensorik, Sendebedingungen, Betriebsstunden.
        // Der Antrieb selbst laeuft ueber drive(), gerufen vom Modul.
    }

    bool KwlFan::drive(KwlOutput& output, uint8_t stage, Direction direction,
                       bool below5vIsSupply)
    {
        if (!mActive)
            return true;

        const VoltPair volt = Curve::stageVolt(mType, stage, direction, below5vIsSupply);

        bool ok;
        if (channelsNeeded() == 2)
        {
            // Sicherheitsinvariante 6: beide Motoren in einem Frame.
            ok = output.setEgo(mDacChannel, volt);
        }
        else
        {
            ok = output.setVolt(mDacChannel, volt.a);
        }

        if (ok)
        {
            mStage = stage > kStageMax ? kStageMax : stage;
            mDirection = direction;
        }
        return ok;
    }

    bool KwlFan::driveSafe(KwlOutput& output)
    {
        if (!mActive)
            return true;

        const float safe = Curve::safeVolt(mType);
        bool ok;
        if (channelsNeeded() == 2)
            ok = output.setEgo(mDacChannel, {safe, safe});
        else
            ok = output.setVolt(mDacChannel, safe);

        if (ok)
            mStage = 0;
        return ok;
    }

    void KwlFan::printStatusLine()
    {
        if (!mActive)
        {
            logInfoP("Kanal %u: nicht aktiv", (unsigned)(_channelIndex + 1));
            return;
        }

        static const char* kTypeName[] = {"e2-60", "ego", "RA15-60", "bipolar", "unipolar"};
        const uint8_t t = static_cast<uint8_t>(mType);

        logInfoP("Kanal %u: %s auf S%u, Raum %u, Verbund %u, Phase %u, %u%%, Stufe %u %s",
                 (unsigned)(_channelIndex + 1),
                 t < 5 ? kTypeName[t] : "?",
                 (unsigned)(mDacChannel + 1),
                 (unsigned)mRoomNo,
                 (unsigned)mGroupNo,
                 (unsigned)mPhase,
                 (unsigned)mShare,
                 (unsigned)mStage,
                 mDirection == Direction::Supply ? "Zuluft" : "Abluft");
    }
} // namespace Kwl
