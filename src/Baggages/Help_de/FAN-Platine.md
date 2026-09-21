### Platine

Wählt die bestückte Platine. Die Auswahl bestimmt, wie viele Lüfterkanäle es
gibt und welcher Kanal an welcher Klemme liegt — das Klemmenbild darunter zeigt es.

Die Firmware vergleicht diese Angabe beim Start mit der tatsächlich bestückten Platine. Stimmen
sie nicht überein, gehen **alle** Lüfter auf Störung mit Fehlercode 3 und ihre Ausgänge bleiben
bei 5,00 V. Eine falsche Auswahl führt also nicht zu stillem Fehlverhalten, sondern zu einer
Meldung.

> Der Eintrag „Entwicklungsaufbau Pico + DFR1073" ist für den Laborbetrieb ohne eigene Platine
> gedacht und hat nur zwei Kanäle.
