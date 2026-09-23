#pragma once

#include "../IDacDrive.h"

namespace Kwl
{
    // ========================================================================
    // Testdouble fuer IDacDrive.
    //
    // Er zeichnet die REIHENFOLGE der Zugriffe auf, nicht nur ihr Ergebnis. Genau
    // darauf kommt es an: dass kein Sollwert vor dem Bereichsregister steht
    // (Invariante 3) und dass der ego in einem Frame gestellt wird (Invariante 6).
    // Ein Mock, der nur den letzten Wert je Kanal merkt, koennte beides nicht
    // zeigen.
    //
    // Er liegt bewusst in src/ und nicht in test/: PlatformIO baut jedes
    // Testverzeichnis fuer sich, und ein gemeinsames Double gehoert dann neben die
    // Schnittstelle, die es erfuellt.
    // ========================================================================
    class MockDacDrive : public IDacDrive
    {
      public:
        enum class Op : uint8_t
        {
            Probe,
            Configure,
            Write,
            WritePair
        };

        struct Entry
        {
            Op op;
            uint8_t channel; ///< bei Probe/Configure ohne Bedeutung
            uint16_t wireA;
            uint16_t wireB; ///< nur bei WritePair
        };

        static constexpr uint8_t kLogMax = 64;

        explicit MockDacDrive(uint8_t channels = 4) : mChannels(channels) {}

        // --- Steuerung des Doubles ------------------------------------------

        /// Laesst probe() fehlschlagen - fuer den Ausfall eines DAC.
        void setPresent(bool present) { mPresent = present; }

        /// Laesst jeden Schreibzugriff fehlschlagen - fuer einen Busabbruch.
        void setWritable(bool writable) { mWritable = writable; }

        void clearLog() { mCount = 0; mOverflow = false; }

        uint8_t count() const { return mCount; }
        bool overflow() const { return mOverflow; }
        const Entry& at(uint8_t index) const { return mLog[index]; }

        /// Index des ersten Eintrags dieser Art, oder -1.
        int16_t firstOf(Op op) const
        {
            for (uint8_t i = 0; i < mCount; i++)
                if (mLog[i].op == op)
                    return static_cast<int16_t>(i);
            return -1;
        }

        /// Zahl der Eintraege dieser Art.
        uint8_t countOf(Op op) const
        {
            uint8_t n = 0;
            for (uint8_t i = 0; i < mCount; i++)
                if (mLog[i].op == op)
                    n++;
            return n;
        }

        /// Zuletzt auf diesen Kanal geschriebener Wert; 0xFFFF, wenn nie.
        uint16_t lastWire(uint8_t channel) const
        {
            uint16_t value = 0xFFFF;
            for (uint8_t i = 0; i < mCount; i++)
            {
                if (mLog[i].op == Op::Write && mLog[i].channel == channel)
                    value = mLog[i].wireA;
                else if (mLog[i].op == Op::WritePair && mLog[i].channel == channel)
                    value = mLog[i].wireA;
                else if (mLog[i].op == Op::WritePair && mLog[i].channel + 1 == channel)
                    value = mLog[i].wireB;
            }
            return value;
        }

        // --- IDacDrive -------------------------------------------------------

        bool probe() override
        {
            record({Op::Probe, 0, 0, 0});
            return mPresent;
        }

        bool configure() override
        {
            record({Op::Configure, 0, 0, 0});
            return mPresent && mWritable;
        }

        bool write(uint8_t channel, uint16_t wire) override
        {
            record({Op::Write, channel, wire, 0});
            return mWritable;
        }

        bool writePair(uint8_t firstChannel, uint16_t wireA, uint16_t wireB) override
        {
            record({Op::WritePair, firstChannel, wireA, wireB});
            return mWritable;
        }

        uint8_t channelCount() const override { return mChannels; }

      private:
        void record(const Entry& e)
        {
            if (mCount >= kLogMax)
            {
                mOverflow = true;
                return;
            }
            mLog[mCount++] = e;
        }

        uint8_t mChannels;
        bool mPresent = true;
        bool mWritable = true;
        Entry mLog[kLogMax] = {};
        uint8_t mCount = 0;
        bool mOverflow = false;
    };
} // namespace Kwl
