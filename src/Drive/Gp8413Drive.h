#pragma once

#include "../IDacDrive.h"

namespace Kwl
{
    // ========================================================================
    // IDacDrive ueber I2C fuer den GP8413 (zwei Kanaele je Chip).
    //
    // Pins, Adressen und Busformat kommen als Konstruktorargumente herein, NICHT
    // aus dem Board-Header. Der Header wird ausschliesslich in Gp8413Drive.cpp
    // eingebunden - und nur dort, damit seine `#error`-Sperre fuer
    // FANDRV_DAC_LEFT_ALIGNED genau eine Uebersetzungseinheit anhaelt und nicht
    // das halbe Projekt. Solange Messung P6 aussteht, ist der Build hier zu Ende;
    // alles andere uebersetzt weiter.
    //
    // Register (Referenzdesign 3.3 und 6.2):
    //   0x01  Bereichswahl, Datenbyte 0x11 = 0…10 V
    //   0x02  VOUT0 schreiben - mit vier Datenbytes VOUT0 UND VOUT1
    //   0x04  VOUT1 schreiben
    // Der Wert geht Low-Byte zuerst hinaus.
    // ========================================================================
    class Gp8413Drive : public IDacDrive
    {
      public:
        /// @param addrA  Adresse des ersten Chips (Kanaele 0 und 1)
        /// @param addrB  Adresse des zweiten Chips (Kanaele 2 und 3)
        /// @param channels  Zahl der bestueckten Stellkanaele (2 oder 4)
        Gp8413Drive(uint8_t addrA, uint8_t addrB, uint8_t channels);

        bool probe() override;
        bool configure() override;
        bool write(uint8_t channel, uint16_t wire) override;
        bool writePair(uint8_t firstChannel, uint16_t wireA, uint16_t wireB) override;
        uint8_t channelCount() const override { return mChannels; }
        uint8_t channelsPerChip() const override { return 2; }

        /// Einmalig die Pins und den Bus aufsetzen. Muss vor probe() laufen.
        void beginBus(uint8_t sdaPin, uint8_t sclPin, uint32_t hz = 100000);

      private:
        uint8_t addressFor(uint8_t channel) const;
        bool writeBytes(uint8_t address, const uint8_t* data, uint8_t length);

        uint8_t mAddr[2];
        uint8_t mChannels;
        bool mBusReady = false;
    };

    /// Der Treiber dieser Platine, aufgebaut aus den Werten des Board-Headers.
    /// Definiert in Gp8413Drive.cpp - der einzigen Stelle, die den Header kennt.
    IDacDrive& boardDacDrive();

    /// Busformat dieser Platine (FANDRV_DAC_LEFT_ALIGNED, Messung P6).
    bool boardLeftAligned();

    /// Zahl der Stellkanaele dieser Platine (FANDRV_BOARD_CHANNELS).
    uint8_t boardChannels();

    /// Kennung der Platine (FANDRV_BOARD_ID). Sie muss zur ETS-Hardwareauswahl
    /// passen, sonst laeuft die Firmware mit falscher Kanalzahl.
    uint8_t boardId();

    /// Polaritaet der Spannungshaelften (FANDRV_BELOW_5V_IS_SUPPLY, Messung M1).
    bool boardBelow5vIsSupply();
} // namespace Kwl
