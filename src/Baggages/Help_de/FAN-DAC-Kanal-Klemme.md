### DAC-Kanal (Klemme)

Der DAC-Kanal und damit die Klemme, an der dieser Lüfter hängt. Welche
Klemme das ist, zeigt das Klemmenbild auf der Seite „Allgemein".

Vorgabe ist Lüfter n an Kanal n. Wer anders verdrahtet hat, stellt es hier um — die Reihenfolge der
Lüfter in der Applikation muss nicht der Reihenfolge der Klemmen entsprechen.

Ein **ego** belegt diesen und den folgenden Kanal. Er muss auf einem ungeraden Kanal beginnen
(1, 3, 5 …), weil beide Motoren auf demselben DAC liegen müssen — nur dann werden sie in einem
einzigen I²C-Frame gestellt und laufen beim Richtungswechsel nicht auseinander.

> Kein Kanal darf doppelt vergeben sein. Die Firmware prüft beim Start auf Doppelbelegung, auf
> Kanäle jenseits der gewählten Platine und auf einen ego mit geradem Startkanal; jeder dieser
> Fälle ergibt Fehlercode 3 am betroffenen Lüfter, und sein Ausgang bleibt bei 5,00 V.
