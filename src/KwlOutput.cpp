#include "KwlOutput.h"

namespace Kwl
{
    void KwlOutput::attach(IDacDrive* drive, bool leftAligned)
    {
        mDrive = drive;
        mLeftAligned = leftAligned;
        mReady = false;
        mError = ErrorCode::None;

        for (uint8_t i = 0; i < kOutputChannelsMax; i++)
        {
            // Ohne Angabe gilt bipolar: das ist der Typ, dessen sicherer Zustand
            // 5,00 V ist. Ein unbekannter Luefter soll stehen, nicht laufen.
            mType[i] = FanType::GENERIC_BIPOLAR;
            mLastVolt[i] = Curve::safeVolt(FanType::GENERIC_BIPOLAR);
        }
    }

    void KwlOutput::setChannelType(uint8_t channel, FanType type)
    {
        if (channel >= kOutputChannelsMax)
            return;
        mType[channel] = type;
        if (!mReady)
            mLastVolt[channel] = Curve::safeVolt(type);
    }

    void KwlOutput::fail(ErrorCode code)
    {
        mReady = false;
        mError = code;
    }

    bool KwlOutput::isPairStart(uint8_t firstChannel) const
    {
        if (mDrive == nullptr)
            return false;
        const uint8_t perChip = mDrive->channelsPerChip();
        if (perChip == 0)
            return false;
        if (firstChannel + 1 >= mDrive->channelCount())
            return false;
        return (firstChannel % perChip) == 0;
    }

    bool KwlOutput::begin()
    {
        if (mDrive == nullptr)
        {
            fail(ErrorCode::Configuration);
            return false;
        }

        // 1. Antwortet ueberhaupt jemand? Ohne Antwort wird nichts geschrieben -
        //    ein Sollwert an einen Chip, der nicht da ist, wird sonst zu einem
        //    Sollwert an irgendetwas anderes auf demselben Bus.
        if (!mDrive->probe())
        {
            fail(ErrorCode::DacUnreachable);
            return false;
        }

        // 2. Bereichsregister ZUERST. Steht der Bereich falsch, bedeutet derselbe
        //    Code eine andere Spannung - auch der des sicheren Zustands.
        if (!mDrive->configure())
        {
            fail(ErrorCode::DacUnreachable);
            return false;
        }

        // 3. Sicherer Zustand auf alle Kanaele, und erst danach ist die Klasse
        //    bereit. Die Reihenfolge ist Sicherheitsinvariante 3.
        const uint8_t channels = mDrive->channelCount();
        for (uint8_t i = 0; i < channels && i < kOutputChannelsMax; i++)
        {
            if (!writeVolt(i, Curve::safeVolt(mType[i])))
            {
                fail(ErrorCode::DacUnreachable);
                return false;
            }
        }

        mReady = true;
        mError = ErrorCode::None;
        return true;
    }

    bool KwlOutput::writeVolt(uint8_t channel, float volt)
    {
        const uint16_t wire = Curve::codeToWire(Curve::voltToCode(volt), mLeftAligned);
        if (!mDrive->write(channel, wire))
            return false;
        mLastVolt[channel] = volt;
        return true;
    }

    bool KwlOutput::setVolt(uint8_t channel, float volt)
    {
        // Kein Sollwert, bevor der Bereich steht.
        if (!mReady || mDrive == nullptr)
            return false;
        if (channel >= mDrive->channelCount() || channel >= kOutputChannelsMax)
            return false;

        if (!writeVolt(channel, volt))
        {
            fail(ErrorCode::DacUnreachable);
            return false;
        }
        return true;
    }

    bool KwlOutput::setEgo(uint8_t firstChannel, const VoltPair& volt)
    {
        if (!mReady || mDrive == nullptr)
            return false;
        if (!isPairStart(firstChannel))
        {
            // Die beiden Motoren laegen auf verschiedenen Chips und liessen sich
            // nicht in einem Frame stellen. Das ist ein Parametrierfehler, kein
            // Grenzfall - und er darf nicht in zwei Einzelschreibvorgaenge
            // ausweichen.
            fail(ErrorCode::Configuration);
            return false;
        }

        const uint16_t wireA = Curve::codeToWire(Curve::voltToCode(volt.a), mLeftAligned);
        const uint16_t wireB = Curve::codeToWire(Curve::voltToCode(volt.b), mLeftAligned);

        if (!mDrive->writePair(firstChannel, wireA, wireB))
        {
            fail(ErrorCode::DacUnreachable);
            return false;
        }

        mLastVolt[firstChannel] = volt.a;
        mLastVolt[firstChannel + 1] = volt.b;
        return true;
    }

    bool KwlOutput::safeAll()
    {
        if (mDrive == nullptr)
            return false;

        // Absichtlich ohne `mReady`: vor einem Neustart oder nach einem Fehler ist
        // der sichere Zustand genau das, was gebraucht wird. Scheitert ein Kanal,
        // werden die uebrigen trotzdem versucht.
        bool ok = true;
        const uint8_t channels = mDrive->channelCount();
        for (uint8_t i = 0; i < channels && i < kOutputChannelsMax; i++)
        {
            if (!writeVolt(i, Curve::safeVolt(mType[i])))
                ok = false;
        }
        if (!ok)
            fail(ErrorCode::DacUnreachable);
        return ok;
    }

    float KwlOutput::lastVolt(uint8_t channel) const
    {
        if (channel >= kOutputChannelsMax)
            return 0.0f;
        return mLastVolt[channel];
    }
} // namespace Kwl
