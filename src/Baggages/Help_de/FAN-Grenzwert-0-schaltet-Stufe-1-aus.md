### Grenzwert 0 — schaltet Stufe 1 aus

Unterster Grenzwert der Treppe. Erreicht der Messwert
diesen Wert nicht mehr, schaltet Stufe 1 wieder ab.

Die Grenzwert-Treppe arbeitet so: Stufe n schaltet **ein**, wenn der Messwert Grenzwert n
erreicht, und wieder **aus**, wenn er unter Grenzwert n−1 fällt. Dadurch liegt zwischen Ein- und
Ausschaltpunkt jeder Stufe immer ein Abstand, und die Anlage pendelt nicht zwischen zwei Stufen.

> Die fünf Grenzwerte müssen aufsteigend sein. Die Firmware prüft das beim Start und meldet
> Fehlercode 3, wenn die Reihenfolge nicht stimmt.
