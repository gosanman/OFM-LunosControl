### Rampe beim Richtungswechsel

Zeit, in der die Stellspannung vor der Totzeit gleichmäßig auf 5,00 V zurückgeht und danach in
der neuen Richtung wieder auf die Stufenspannung steigt. Die Zeit gilt je Rampe, also einmal
hinunter und einmal hinauf.

Ablauf eines Wechsels: Stufenspannung → Rampe auf 5,00 V → Totzeit → Rampe in die neue Richtung.
Die Zykluszeit zählt ab der Übernahme der neuen Richtung, Rampe und Totzeit verlängern den Takt
also nicht auf Kosten der Förderzeit in der neuen Richtung.

**0 s** schaltet die Rampe ab: Die Spannung springt wie früher direkt auf 5,00 V und nach der
Totzeit auf die neue Stufe.

Die Rampe wirkt nur beim Richtungswechsel. Ein Stufenwechsel in derselben Richtung springt
weiterhin sofort, damit Sperre, Schutz und Stoßlüften ohne Verzögerung greifen. Läuft der
Verbund aus dem Stillstand an, gibt es nichts herunterzufahren. Dann folgen nur Totzeit und
Rampe nach oben.

> Das Statusobjekt „Ausgangsspannung“ meldet die Zielspannung, nicht jeden Zwischenwert der
> Rampe.
