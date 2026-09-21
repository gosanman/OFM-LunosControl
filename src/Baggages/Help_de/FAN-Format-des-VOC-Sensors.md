### Format des VOC-Sensors

Datenformat des angeschlossenen Luftgütesensors. Danach richtet
sich, welches Kommunikationsobjekt erscheint und in welcher Einheit die Grenzwerte gelten.

* **ppb (DPT 9.008)** — Konzentration in Teilen pro Milliarde, 2 Byte.
* **Index 0…255 (DPT 5.010)** — ein einheitenloser Luftgüteindex, 1 Byte.
* **Index 0…65535 (DPT 7.001)** — derselbe Gedanke mit größerem Bereich, 2 Byte.

Ein Kommunikationsobjekt hat genau eine Größe, deshalb gibt es für die 1-Byte-Variante ein eigenes
Objekt. Sichtbar ist immer nur das zum Format passende.
