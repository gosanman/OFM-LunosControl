### Überwachungszeit Master

Zeit, nach der ein Verbund mit der Rolle Slave den externen
Master als ausgefallen betrachtet, wenn kein Lebenszeichen mehr kommt.

Läuft die Zeit ab, gehen die Lüfter dieses Verbunds auf Stillstand (5,00 V) und melden
Fehlercode 2. Das ist Absicht: ein Slave ohne Vorgabe darf nicht auf gut Glück weiterfördern,
weil er die Richtung des Partners nicht kennt und die Bilanz im Haus sonst kippt.
