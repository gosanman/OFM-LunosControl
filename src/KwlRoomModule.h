#pragma once

#include "KwlRoom.h"
#include "OpenKNX.h"
#include "knxprod.h"

namespace Kwl
{
    // ========================================================================
    // OpenKNX-Modul der Raeume.
    //
    // Es steht neben dem Luefter-Modul und nicht darunter: ein Raum kennt seine
    // Luefter nicht, er liefert einen Stufenwunsch. Das Luefter-Modul liest ihn
    // in-process ab - nie ueber den Bus.
    // ========================================================================
    class KwlRoomModule : public OpenKNX::Module
    {
      public:
        void setup(bool configured) override;
        void loop(bool configured) override;
        void processInputKo(GroupObject& ko) override;

        void showHelp() override;
        bool processCommand(const std::string cmd, bool debugKo) override;

        const std::string name() override { return "KwlRoom"; }
        const std::string version() override { return "0.1"; }

        /// Raum nach ETS-Nummer (1…8), oder nullptr.
        KwlRoom* room(uint8_t roomNo);

      private:
        void printStatus();

        KwlRoom* mRoom[ROOM_ChannelCount] = {};
    };
} // namespace Kwl

extern Kwl::KwlRoomModule openknxKwlRoomModule;
