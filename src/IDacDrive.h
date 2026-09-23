#pragma once

#include "KwlTypes.h"

namespace Kwl
{
    // ========================================================================
    // Ausgabe eines 15-Bit-Werts auf einen Stellkanal.
    //
    // Diese Schnittstelle beschreibt WAS geschrieben wird, nicht WIE. Sie kennt
    // keine Pins, keine Adressen und kein Protokoll - eine Implementierung bringt
    // ihre Konfiguration im Konstruktor mit, weil ein GP8413 am I2C etwas anderes
    // braucht als ein Mock im Test.
    //
    // Die Werte sind bereits fertig: KwlOutput rechnet Volt in den logischen Code
    // und danach in das Busformat um. Der Treiber rundet nicht, klemmt nicht und
    // schiebt nicht - er schreibt.
    //
    // WICHTIG: writePair() ist keine Bequemlichkeit, sondern Sicherheitsinvariante
    // 6. Der ego hat zwei Motoren an einem Chip; sie muessen in EINEM Frame
    // gestellt werden (Register 0x02, vier Datenbytes). Zwei getrennte Schreib-
    // vorgaenge lassen die Motoren beim Richtungswechsel auseinanderlaufen.
    // ========================================================================
    class IDacDrive
    {
      public:
        virtual ~IDacDrive() = default;

        /// Adressabfrage an alle Chips. false heisst: mindestens einer antwortet
        /// nicht. Der Knoten geht dann auf Fehlercode, Alarm und rote LED - kein
        /// stilles Weiterrechnen (Sicherheitsinvariante 10).
        virtual bool probe() = 0;

        /// Bereichsregister setzen: Register 0x01 = 0x11 an JEDEN Chip, also
        /// 0…10 V auf beiden Ausgaengen. Muss vor jedem Sollwert laufen
        /// (Sicherheitsinvariante 3).
        virtual bool configure() = 0;

        /// Einen Kanal schreiben. `wire` ist der Wert so, wie er auf den Bus geht.
        virtual bool write(uint8_t channel, uint16_t wire) = 0;

        /// Beide Kanaele eines Chips in einem einzigen Frame.
        /// `firstChannel` ist der niedrigere der beiden; er muss der erste Kanal
        /// eines Chips sein.
        virtual bool writePair(uint8_t firstChannel, uint16_t wireA, uint16_t wireB) = 0;

        /// Zahl der Stellkanaele dieses Treibers.
        virtual uint8_t channelCount() const = 0;

        /// Kanaele je Chip - danach richtet sich, welche Kanaele ein Paar bilden.
        virtual uint8_t channelsPerChip() const { return 2; }
    };
} // namespace Kwl
