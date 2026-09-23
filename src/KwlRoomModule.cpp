#include "KwlRoomModule.h"

Kwl::KwlRoomModule openknxKwlRoomModule;

namespace Kwl
{
    void KwlRoomModule::setup(bool configured)
    {
        if (!configured)
            return;

        for (uint8_t i = 0; i < ROOM_ChannelCount; i++)
        {
            if (mRoom[i] == nullptr)
                mRoom[i] = new KwlRoom(i);
            mRoom[i]->setup();
        }
    }

    void KwlRoomModule::loop(bool configured)
    {
        if (!configured)
            return;

        for (uint8_t i = 0; i < ROOM_ChannelCount; i++)
            if (mRoom[i] != nullptr)
                mRoom[i]->loop();
    }

    void KwlRoomModule::processInputKo(GroupObject& ko)
    {
        // KoCalcChannel rechnet ohne _channelIndex und laesst sich deshalb im
        // Modul benutzen; KoCalcIndex braucht einen Kanal und gehoert dorthin.
        const int channel = ROOM_KoCalcChannel(ko.asap());
        if (channel < 0 || channel >= ROOM_ChannelCount)
            return;

        if (mRoom[channel] != nullptr)
            mRoom[channel]->processInputKo(ko);
    }

    KwlRoom* KwlRoomModule::room(uint8_t roomNo)
    {
        if (roomNo < 1 || roomNo > ROOM_ChannelCount)
            return nullptr;
        return mRoom[roomNo - 1];
    }

    void KwlRoomModule::printStatus()
    {
        logInfoP("Raeume:");
        for (uint8_t i = 0; i < ROOM_ChannelCount; i++)
            if (mRoom[i] != nullptr)
                mRoom[i]->printStatusLine();
    }

    void KwlRoomModule::showHelp()
    {
        openknx.console.printHelpLine("kwlr", "Raeume anzeigen");
    }

    bool KwlRoomModule::processCommand(const std::string cmd, bool debugKo)
    {
        (void)debugKo;

        if (cmd == "kwlr")
        {
            printStatus();
            return true;
        }
        return false;
    }
} // namespace Kwl
