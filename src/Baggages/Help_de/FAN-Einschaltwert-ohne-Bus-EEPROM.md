### Einschaltwert ohne Bus (EEPROM)

Stufe, die dieser Kanal einnimmt, wenn die 12-V-Versorgung
ohne KNX-Busspannung wiederkehrt.

Der Wert liegt im EEPROM des DAC, nicht in der Firmware — er gilt also auch dann, wenn der
Prozessor nicht arbeitet. Stufe 1 ist die sinnvolle Vorgabe, weil sie den Feuchteschutz sicher
stellt.

> Geschrieben wird der Wert **ausschließlich von Hand** über den Konsolenbefehl `kwl store`, und
> auch dort erst nach einer Rückfrage. Er wird nie im Betrieb, nie durch ein Kommunikationsobjekt
> und nie automatisch gesetzt: die Zahl der Schreibzyklen des Bausteins ist unbekannt.
