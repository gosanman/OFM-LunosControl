#include "KwlFanModule.h"

#include "Drive/Gp8413Drive.h"

Kwl::KwlFanModule openknxKwlFanModule;

namespace Kwl
{
    bool KwlFanModule::checkHardware()
    {
        // Die ETS bietet mehr Platinen an, als in diesem Geraet steckt. Stimmen
        // Auswahl und Bestueckung nicht ueberein, waeren alle Kanalzuordnungen
        // verschoben - und eine verschobene Zuordnung stellt den falschen Luefter
        // auf Vollgas. Deshalb Fehlercode 3 und kein stilles Weiterlaufen.
        if (ParamFAN_Hardware == boardId())
            return true;

        logErrorP("Hardwareauswahl %u passt nicht zur Platine (%u, %u Kanaele) - "
                  "alle Ausgaenge bleiben im sicheren Zustand",
                  (unsigned)ParamFAN_Hardware, (unsigned)boardId(),
                  (unsigned)boardChannels());
        mError = ErrorCode::Configuration;
        return false;
    }

    void KwlFanModule::setupChannels()
    {
        for (uint8_t i = 0; i < FAN_ChannelCount; i++)
        {
            if (mFan[i] == nullptr)
                mFan[i] = new KwlFan(i);
            mFan[i]->setup();

            // Der Typ entscheidet ueber den sicheren Zustand des DAC-Kanals, muss
            // also VOR KwlOutput::begin() stehen.
            if (mFan[i]->isActive())
            {
                mOutput.setChannelType(mFan[i]->dacChannel(), mFan[i]->type());
                if (mFan[i]->channelsNeeded() == 2)
                    mOutput.setChannelType(mFan[i]->dacChannel() + 1, mFan[i]->type());
            }
        }
    }

    void KwlFanModule::setup(bool configured)
    {
        mBelow5vIsSupply = boardBelow5vIsSupply();
        mOutput.attach(&boardDacDrive(), boardLeftAligned());

        if (!configured)
        {
            // Ohne gueltige Parametrierung wird nicht gefahren - aber die Ausgaenge
            // muessen trotzdem definiert stehen.
            mOutput.begin();
            mOutput.safeAll();
            return;
        }

        if (!checkHardware())
        {
            mOutput.begin();
            mOutput.safeAll();
            return;
        }

        setupChannels();

        // Reihenfolge nach Sicherheitsinvariante 3: Bereichsregister, sicherer
        // Zustand, erst danach Sollwerte. begin() macht genau das.
        if (!mOutput.begin())
        {
            mError = mOutput.error();
            logErrorP("DAC nicht erreichbar - Fehlercode %u", (unsigned)mError);
            return;
        }

        logInfoP("%u Stellkanaele bereit, Busformat %s", (unsigned)boardChannels(),
                 boardLeftAligned() ? "linksbuendig" : "rechtsbuendig");
    }

    void KwlFanModule::loop(bool configured)
    {
        if (!configured || mError != ErrorCode::None)
            return;

        // Woche 3: Verbuende rechnen, Stufen der Raeume abholen, Luefter stellen.
        // Bis dahin steht hier nichts - und genau deshalb stehen die Ausgaenge auf
        // dem sicheren Zustand, den begin() geschrieben hat.
    }

    void KwlFanModule::processBeforeRestart()
    {
        // Sicherheitsinvariante 8: eine ETS-Neuprogrammierung darf keinen
        // Vollgas-Moment erzeugen.
        mOutput.safeAll();
        logInfoP("Sicherer Zustand vor Neustart geschrieben");
    }

    void KwlFanModule::printStatus()
    {
        logInfoP("Luefterkanaele:");
        for (uint8_t i = 0; i < FAN_ChannelCount; i++)
            if (mFan[i] != nullptr)
                mFan[i]->printStatusLine();

        logInfoP("Busformat %s, Platine %u mit %u Kanaelen, Fehlercode %u",
                 boardLeftAligned() ? "linksbuendig" : "rechtsbuendig",
                 (unsigned)boardId(), (unsigned)boardChannels(),
                 (unsigned)mError);
    }

    void KwlFanModule::showHelp()
    {
        openknx.console.printHelpLine("kwl", "Lueftersteuerung anzeigen");
    }

    bool KwlFanModule::processCommand(const std::string cmd, bool debugKo)
    {
        (void)debugKo;

        if (cmd.rfind("kwl", 0) != 0)
            return false;

        if (cmd == "kwl")
        {
            openknx.console.printHelpLine("kwl st", "Alle Luefterkanaele je eine Zeile");
            return true;
        }

        if (cmd == "kwl st")
        {
            printStatus();
            return true;
        }

        return false;
    }
} // namespace Kwl
