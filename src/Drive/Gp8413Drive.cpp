#include "Gp8413Drive.h"

// Die EINZIGE Uebersetzungseinheit, die den Board-Header kennt. Damit haelt die
// `#error`-Sperre fuer FANDRV_DAC_LEFT_ALIGNED genau hier an und nicht ueberall.
#include "hardware.h"

#include <hardware/gpio.h>
#include <hardware/i2c.h>
#include <pico/time.h>

namespace Kwl
{
    namespace
    {
        constexpr uint8_t kRegRange = 0x01;  ///< Bereichswahl
        constexpr uint8_t kRegVout0 = 0x02;  ///< VOUT0, mit vier Datenbytes auch VOUT1
        constexpr uint8_t kRegVout1 = 0x04;  ///< VOUT1
        constexpr uint8_t kRange0to10V = 0x11;

        /// Startzeit des GP8413 nach Anlegen der 12 V (< 2 ms) plus Reserve,
        /// Referenzdesign 6.1 Punkt 1.
        constexpr uint32_t kStartupMs = 5;

        /// Wiederholungen der Adressabfrage. Gemessen 2026-09-25 (Messprotokoll
        /// Phase 1): U3 antwortete nach dem Kaltstart erst beim dritten Anlauf.
        constexpr uint8_t kProbeAttempts = 3;
        constexpr uint32_t kProbeRetryMs = 10;
    } // namespace

    Gp8413Drive::Gp8413Drive(uint8_t addrA, uint8_t addrB, uint8_t channels)
        : mAddr{addrA, addrB}, mChannels(channels)
    {
    }

    void Gp8413Drive::beginBus(uint8_t sdaPin, uint8_t sclPin, uint32_t hz)
    {
        i2c_init(FANDRV_I2C_PORT, hz);
        gpio_set_function(sdaPin, GPIO_FUNC_I2C);
        gpio_set_function(sclPin, GPIO_FUNC_I2C);
        // Die Pull-ups sitzen als R4/R5 auf der Platine; die internen sind
        // absichtlich AUS. Zwei Pull-up-Paare parallel verschieben die Flanken,
        // und der ADuM1250 sieht ohnehin nur die aeusseren.
        mBusReady = true;
    }

    uint8_t Gp8413Drive::addressFor(uint8_t channel) const
    {
        return mAddr[channel / 2];
    }

    bool Gp8413Drive::writeBytes(uint8_t address, const uint8_t* data, uint8_t length)
    {
        if (!mBusReady)
            return false;
        const int written = i2c_write_blocking(FANDRV_I2C_PORT, address, data, length, false);
        return written == static_cast<int>(length);
    }

    bool Gp8413Drive::probe()
    {
        if (!mBusReady)
            return false;

        sleep_ms(kStartupMs);

        // Adressabfrage per SCHREIBZUGRIFF, nicht per Lesezugriff. Gemessen
        // 2026-09-25 (Messprotokoll Phase 1, vor E1 und E2): U3 quittierte nach
        // dem Kaltstart mehrfach keinen Lesezugriff, einen Schreibzugriff sofort.
        // Der RP2040 kann keinen Schreibzugriff ohne Nutzdaten senden (das
        // Synopsys-I2C kennt keinen Null-Byte-Transfer), deshalb Register 0x01 <-
        // 0x11: idempotent und ohnehin das Erste, was configure() schreibt - die
        // Reihenfolge aus Invariante 3 bleibt gewahrt. Der Lesepfad liefert beim
        // GP8413 ohnehin nur 0x11 zurueck (O6, F2).
        const uint8_t buf[2] = {kRegRange, kRange0to10V};
        for (uint8_t chip = 0; chip < 2; chip++)
        {
            if (chip * 2 >= mChannels)
                break;
            bool ok = false;
            for (uint8_t attempt = 0; attempt < kProbeAttempts && !ok; attempt++)
            {
                if (attempt > 0)
                    sleep_ms(kProbeRetryMs);
                ok = writeBytes(mAddr[chip], buf, sizeof(buf));
            }
            if (!ok)
                return false;
        }
        return true;
    }

    bool Gp8413Drive::configure()
    {
        // Bereich 0…10 V an JEDEN bestueckten Chip. Der Schreibvorgang ist
        // idempotent und kostet nichts - er laeuft auch dann, wenn der EEPROM den
        // Bereich schon haelt (Referenzdesign 6.1, offener Punkt O1).
        const uint8_t buf[2] = {kRegRange, kRange0to10V};
        for (uint8_t chip = 0; chip < 2; chip++)
        {
            if (chip * 2 >= mChannels)
                break;
            if (!writeBytes(mAddr[chip], buf, sizeof(buf)))
                return false;
        }
        return true;
    }

    bool Gp8413Drive::write(uint8_t channel, uint16_t wire)
    {
        if (channel >= mChannels)
            return false;

        uint8_t buf[3];
        buf[0] = (channel % 2 == 0) ? kRegVout0 : kRegVout1;
        buf[1] = static_cast<uint8_t>(wire & 0xFF); // Low-Byte zuerst
        buf[2] = static_cast<uint8_t>(wire >> 8);
        return writeBytes(addressFor(channel), buf, sizeof(buf));
    }

    bool Gp8413Drive::writePair(uint8_t firstChannel, uint16_t wireA, uint16_t wireB)
    {
        // Sicherheitsinvariante 6: beide Motoren des ego in EINEM Frame, vier
        // Datenbytes mit ZWEI verschiedenen Werten. Die Bibliotheksfunktion von
        // DFRobot schreibt hier denselben Wert in beide Ausgaenge und ist dafuer
        // unbrauchbar - das Register aber sehr wohl (Referenzdesign 6.2).
        if (firstChannel + 1 >= mChannels || firstChannel % 2 != 0)
            return false;

        uint8_t buf[5];
        buf[0] = kRegVout0;
        buf[1] = static_cast<uint8_t>(wireA & 0xFF);
        buf[2] = static_cast<uint8_t>(wireA >> 8);
        buf[3] = static_cast<uint8_t>(wireB & 0xFF);
        buf[4] = static_cast<uint8_t>(wireB >> 8);
        return writeBytes(addressFor(firstChannel), buf, sizeof(buf));
    }

    // ------------------------------------------------------------ Board

    IDacDrive& boardDacDrive()
    {
        static Gp8413Drive drive(FANDRV_DAC_ADDR_A, FANDRV_DAC_ADDR_B,
                                 FANDRV_BOARD_CHANNELS);
        static bool started = false;
        if (!started)
        {
            drive.beginBus(FANDRV_I2C_SDA, FANDRV_I2C_SCL);
            started = true;
        }
        return drive;
    }

    bool boardLeftAligned() { return FANDRV_DAC_LEFT_ALIGNED != 0; }
    uint8_t boardChannels() { return FANDRV_BOARD_CHANNELS; }
    uint8_t boardId() { return FANDRV_BOARD_ID; }
    bool boardBelow5vIsSupply() { return FANDRV_BELOW_5V_IS_SUPPLY != 0; }
} // namespace Kwl
