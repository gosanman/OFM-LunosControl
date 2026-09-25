# KNX Lüftersteuerung 0–10 V — Applikationsbeschreibung

Für dezentrale Lüftungsgeräte mit 0–10-V-Stellsignal, entwickelt an LUNOS e²60,
ego und RA 15-60. Vier Stellkanäle S1…S4, bis zu 8 Räume, 12 Lüfter und
8 Verbünde.

Applikation `KwlControl`, Bestellnummer KNXFANDRV.

---

## Bevor Sie anschließen

**5,00 V ist Stillstand. 0 V ist Volllast.** Das Stellsignal der bipolaren Geräte
ist um 5 V herum aufgebaut: unterhalb fördert der Lüfter in die eine Richtung,
oberhalb in die andere, und je weiter von 5 V entfernt, desto schneller. Ein
Stellsignal von 0 V bedeutet **nicht** „aus", sondern volle Drehzahl.

**Nach dem Einschalten der 12 V laufen die Lüfter kurz mit voller Drehzahl.** Die
verwendeten Wandler halten keinen Einschaltwert; die Ausgänge führen 0 V, bis die
Firmware hochgelaufen ist und den sicheren Zustand geschrieben hat. Das dauert
unter einer Sekunde.

> **Liegen 12 V an, ohne dass der KNX-Bus da ist, bleibt es dabei.** Die Lüfter
> laufen dann dauerhaft mit voller Drehzahl. Versorgen Sie die 12 V nicht
> unabhängig vom Bus, oder schalten Sie sie mit.

**KNX ist kein Sicherheitsbus.** Für den Verbund mit einer Feuerstätte ist
zusätzlich ein fest verdrahteter Kontakt gefordert. Das Objekt „Sperre" ersetzt
ihn nicht.

---

## Die drei Ebenen

Das Gerät trennt, was oft vermischt wird:

| Ebene | Was sie entscheidet | ETS |
|---|---|---|
| **Raum** | Was gelüftet werden *soll*: Sensoren, Betriebsarten, Bedienung, Führungen | bis 8 Kanäle |
| **Verbund** | Wie zwei oder mehr Lüfter *zusammen* arbeiten: Stufenregel, Takt, Richtung | bis 8, im Lüftermodul |
| **Lüfter** | Wie das Gerät *angesteuert* wird: Typ, Kennlinie, Stellkanal, Kompensation | bis 12 Kanäle |

Ein Lüfter gehört zu genau **einem** Raum und **einem** Verbund. Ein Raum kann
mehrere Lüfter haben, ein Lüfter aber nur einen Raum — und zwei Lüfter im selben
Verbund laufen immer gegenläufig.

**Es müssen nicht zwei Lüfter in einem Raum sein.** Ein einzelner Lüfter im
Schlafzimmer und einer im Flur bilden zusammen einen Verbund: was der eine
hineindrückt, zieht der andere heraus. Die Zuordnung zu Räumen und die zu
Verbünden sind unabhängig voneinander.

---

## Inbetriebnahme in fünf Schritten

1. **Platine wählen** (Seite *Allgemein*). Stimmt die Auswahl nicht mit der
   bestückten Hardware überein, gehen alle Lüfter auf Störung mit Fehlercode 3
   und die Ausgänge bleiben auf 5,00 V. Das ist Absicht: eine verschobene
   Kanalzuordnung stellt den falschen Lüfter auf Vollgas.
2. **Kanäle aktivieren** (Seite *Kanalauswahl*): Räume, Lüfter, Verbünde.
3. **Je Lüfter** Typ und Stellkanal festlegen, dann Raum, Verbund und Phase.
   Beide Lüfter eines Verbunds brauchen **verschiedene Phasen** (0 und 1) — sonst
   laufen sie gleichsinnig und es gibt weder Luftaustausch noch Wärmerückgewinnung.
4. **Je Raum** die Sensoren verknüpfen und die Betriebsarten parametrieren.
5. **Prüfen** über die serielle Konsole: `kwl st`, `kwl r1`, `kwl f1`.

### Balance — der Punkt, an dem Anlagen scheitern

**Zuluft- und Abluftseite müssen sich die Waage halten.** Fördert eine Seite mehr
als die andere, entsteht Über- oder Unterdruck, und der Rest geht durch Fugen,
Fenster und Kamine — an der Wärmerückgewinnung vorbei.

Praktisch heißt das:

- Gleich viele Geräte mit gleicher Stufe je Phase, oder
- den **Anteilsfaktor** (25…100 %) benutzen, wenn die Zahlen nicht aufgehen. Bei
  drei gleichen Lüftern etwa einer voll und zwei zu 50 %.
- Der **ego** zählt für sich: er hat zwei Motoren und pendelt intern, braucht
  also keinen Partner für die Balance — wohl aber, wenn er in Stufe 4 mit beiden
  Motoren Abluft fördert. Dafür gibt es die Zuluftanforderung.

---

## Betriebsarten

Sieben Stück, je Raum getrennt parametrierbar. Vier davon sind KNX-Standard
(DPT 20.102), drei erweitert.

| Wert | Betriebsart | Vorgabe Grundstufe | Vorgabe Maximalstufe |
|---|---|---|---|
| 1 | Komfort | 1 | 4 |
| 2 | Standby | 1 | 2 |
| 3 | Nacht | 1 | **1** |
| 4 | Frost-/Gebäudeschutz | 0 | 1 |
| 5 | Stoßlüften | 4 | 4 |
| 6 | Temperatur-Absenkung | 1 | 2 |
| 7 | Ruhe (Aus) | 0 | 0 |

**Grundstufe** läuft immer, auch ohne jede Anforderung. **Maximalstufe** ist der
Deckel für alle Führungen — der eigentliche Zweck der Betriebsart *Nacht*: im
Schlafzimmer soll auch ein CO₂-Wert über allen Grenzwerten nicht Stufe 4 auslösen.

Je Betriebsart lässt sich zusätzlich einstellen, welche Führungen überhaupt laufen
(Temperatur, Feuchte, CO₂, Luftgüte, Entfeuchtung, Frostschutz), welche
Zyklusregel gilt, ob Intervallbetrieb läuft und wie lange der Nachlauf ist.

---

## Die Vorfahrt

Die wirksame Stufe entsteht an **einer** Stelle nach dieser Tabelle. Was weiter
oben steht, gewinnt.

| Rang | Ebene | Wirkung |
|---|---|---|
| 1 | **Sperre** | Stillstand (oder Grundstufe, per Parameter). Gewinnt gegen alles. |
| 2 | **Schutz** (Frost/Hitze) | Schutzstufe. Gewinnt auch gegen Handbedienung. |
| 3 | **Zu-/Abluftanforderung, Betriebsweise** | Erzwingt Richtung, die Anforderung auch die Stufe. |
| 4 | **Verbund** | Slave folgt dem Master. |
| 5 | **Handstufe** | Ersetzt die Automatikstufe. |
| 6 | **Automatikstufe** | Grundstufe, angehoben durch die Führungen, gedeckelt durch die Maximalstufe. |
| 7 | Zwangsobjekt 1 / 2 / 3 | bestimmen, *welcher* Parametersatz |
| 8 | Nacht | in Rang 6 gilt |
| 9 | Zwangsbetriebsart | |
| 10 | Betriebsart | |
| 11 | Standard-Betriebsart | |

**Abweichung vom Arcus-Vorbild:** Dort ist der Frostschutz eine Führung unter
anderen. Hier steht er auf Rang 2 und gewinnt gegen Hand und Verbund — ein
durchfrierender Raum ist kein Bedienfall.

**Die Maximalstufe deckelt die Handstufe nicht.** Sie ist der Deckel für die
Automatik. Wer nachts bewusst Stufe 3 drückt, bekommt Stufe 3.

### Rücksetzen

Ändert sich ein Objekt der Ränge 5 bis 11, werden alle Ebenen höheren Rangs bis
einschließlich 5 zurückgesetzt. Jede Bedienung bekommt so eine sichtbare Reaktion.
Die Ränge 1 bis 4 sind ausgenommen — eine Sperre lässt sich nicht wegdrücken.

Zwei Beispiele:

- *Auto → Nacht → Zwangsobjekt 1*: Das Zwangsobjekt gilt. Nach Ablauf seiner
  Laufzeit gilt wieder Nacht.
- *Auto → Zwangsobjekt 1 → Nacht*: Die Nacht löscht das Zwangsobjekt. Nach
  Rücknahme der Nacht gilt wieder Auto.

---

## Führungen

Alle Führungen arbeiten über eine **Grenzwert-Treppe**: fünf Grenzwerte für vier
Stufen. Grenzwert *n* schaltet Stufe *n* ein, Grenzwert *n−1* schaltet sie wieder
aus. Die Hysterese ist der Abstand der Grenzwerte und braucht keinen eigenen
Parameter.

| Führung | Vorgaben |
|---|---|
| Relative Feuchte innen | 45 / 50 / 55 / 60 / 65 % |
| CO₂ | 700 / 850 / 1000 / 1300 / 1700 ppm |
| Luftgüte (VOC), ppb | 300 / 500 / 750 / 1000 / 1500 |
| Luftgüte, Index | 100 / 150 / 200 / 300 / 400 |

Es gewinnt die höchste Stufe, die eine Führung verlangt. Die Maximalstufe der
Betriebsart deckelt das Ergebnis.

### Feuchtevergleich

Lüften trocknet nur, wenn die Außenluft weniger Wasser enthält. Verglichen wird
der **Wasserdampf-Partialdruck**, nicht die relative Feuchte: 80 % bei 0 °C sind
trockener als 50 % bei 20 °C.

Liegt der Partialdruck innen um mindestens 1,5 hPa über dem außen, wirkt
Entfeuchten und die Feuchteführung läuft. Unter 0,5 hPa Unterschied wird sie
gesperrt, das Objekt *Feuchtevergleich sperrt* geht auf 1, und es bleibt bei der
Grundstufe. Dazwischen bleibt der letzte Zustand stehen.

> Das ist der Sommer-Kellerfall: innen 18 °C und 75 %, draußen 28 °C und 60 %.
> Gefühlt ist es draußen trockener — tatsächlich steht dort mehr Wasser in der
> Luft, und Lüften würde den Keller feuchter machen.

Ohne gültige Außenwerte bleibt die Feuchteführung gesperrt. Nicht gemessen heißt
nicht entfeuchten.

Zur Anzeige gibt das Gerät die absolute Feuchte innen und außen in g/kg aus. Dafür
— und nur dafür — braucht es den Parameter *Höhe über Meer*.

### Temperaturführung

| Fall | Bedingung | Stufe | Takt |
|---|---|---|---|
| Freie Kühlung | innen > Soll + 1 K, außen kühler als innen − Abstand | „Kühlung" (Vorgabe 3) | Sommer |
| Wärmeerhalt | innen < Soll, außen kühler als innen − Abstand | keine | WRG |
| Warmluft nutzen | innen < Soll − 1 K, außen wärmer als innen + Abstand | „Heizung" (Vorgabe 2) | Sommer |

Der **Temperaturabstand** (Vorgabe 3 K) hält die Führung der Heizungsregelung aus
dem Weg: erst wenn innen und außen weiter auseinanderliegen, greift sie ein.

Das eine Kelvin zwischen den Fällen ist die Hysterese — die Führung hält dort
ihren Zustand, statt zwischen den Takten zu springen.

**Frostschutz** (Vorgabe 8 °C) schaltet die Lüftung ab und gibt erst 2 K über dem
Grenzwert wieder frei. **Hitzeschutz** (Vorgabe 30 °C) greift nur, wenn es draußen
noch wärmer ist — sonst wäre Kühlen ja richtig.

### Fehlende Messwerte

Jeder Sensoreingang hat eine Überwachungszeit. Bleibt der Wert aus, gilt er als
nicht vorhanden, und der Parameter *bei fehlenden Messwerten* entscheidet:
Grundstufe weiterfahren oder Stillstand.

---

## Verbund, Takt und Wärmerückgewinnung

**Die Pendelbewegung *ist* die Wärmerückgewinnung.** Bei Geräten mit Regenerator
gibt es dafür keinen Schalter: der Speicher nimmt die Wärme der Abluft auf und
gibt sie an die Zuluft ab, wenn die Richtung wechselt.

| Zyklusregel | Zykluszeit | WRG | wofür |
|---|---|---|---|
| Wärmerückgewinnung | je Stufe, Vorgabe 70 s | ja | Heizperiode |
| Sommer | Vorgabe 1 h | praktisch nein | Sommer, freie Kühlung |
| über Objekt „Sommer" | schaltet zwischen beiden | | Standard |
| feste Richtung Zuluft/Abluft | kein Wechsel | nein | Sonderfälle |

**Sommerbetrieb ist kein Einrichtungsbetrieb, sondern ein langer Zyklus.** Der
Regenerator ist nach etwa einer Minute gesättigt; bei einer Stunde Zykluszeit
überträgt er praktisch keine Wärme mehr — die Richtung wechselt aber weiter, und
damit gibt es keinen dauerhaften Über- oder Unterdruck und beide Räume bekommen
Frischluft.

**Vor jedem Richtungswechsel** stehen alle Kanäle des Verbunds für die *Totzeit*
(Vorgabe 2 s) auf 5,00 V. Beim ego gilt das für beide Motoren gleichzeitig.

### Wenn die Räume uneins sind

| Konflikt | Regel |
|---|---|
| Verschiedene Stufenwünsche | Parameter *Stufenregel*: Maximum mit kleinstem Raumdeckel (Vorgabe), Minimum, oder ein Raum führt |
| Verschiedene Zykluswünsche | Parameter: kürzester Zyklus gewinnt (Vorgabe), oder der führende Raum entscheidet |
| Verschiedene Richtungsforderungen | Die höhere Stufe führt; bei Gleichstand die niedrigere Raumnummer. Der unterlegene Raum meldet Fehlercode 11. |

Ein Verbund hat genau **eine** Richtung. Zwei Lüfter darin können nie gleichsinnig
laufen — was in den einen Raum hineingedrückt wird, muss aus dem anderen heraus.

### Master und Slave

Ein Verbund kann über mehrere Geräte gehen. Die Rolle steht je Verbund:

- **intern** — nur Lüfter dieses Geräts,
- **Master** — sendet Stufe, Halbwelle und Lebenszeichen auf den Bus,
- **Slave** — folgt einem fremden Master.

Bleibt beim Slave das Lebenszeichen länger als die Überwachungszeit aus, gehen
seine Lüfter auf Stillstand und melden Fehlercode 2. Er weiß dann nicht mehr, in
welcher Halbwelle der Verbund steht, und Weiterlaufen hieße raten.

Die Verbund-Objekte liegen beim Lüfter mit der **kleinsten Kanalnummer** des
jeweiligen Verbunds.

---

## Abluftanforderung — die Badlüftung

Das Objekt *Abluftanforderung* fährt den Raum in Abluft mit der parametrierten
Stufe (Vorgabe 4; beim ego ist Stufe 4 der Abluftstoß beider Motoren).

- **Vorlaufzeit** (0 s…5 min): erst nach dieser Zeit wird gefördert. Wird die
  Anforderung vorher zurückgenommen, passiert nichts — es gibt keinen Nachlauf für
  eine Lüftung, die nie lief.
- **Nachlaufzeit** (Vorgabe 15 min): danach läuft es weiter.
- **Intermittierend** (etwa 1 min je 5 min): in den Pausen fällt der Raum auf
  seine Automatik zurück, die Grundlüftung läuft also weiter.

Solange Abluft läuft, sendet der Raum eine **Zuluftanforderung**. Verbünde mit dem
Parameter *folgt Zuluftanforderung von Raum n* schalten für die Dauer auf Zuluft —
der ego im Bad zieht 45 m³/h ab, und die muss irgendwo nachströmen, sonst pfeift
es an den Fenstern.

---

## Kommunikationsobjekte

### Je Raum (Block 40, ab KO 20)

| Nr. | Objekt | Richtung | DPT |
|---|---|---|---|
| 0 | Betriebsart (0 = Automatik) | Ein | 20.102 |
| 1 | Zwangsbetriebsart, mit Laufzeit | Ein | 20.102 |
| 2 | Nacht | Ein | 1.003 |
| 3–5 | Zwangsobjekt 1 / 2 / 3 | Ein | 1.003 |
| 6 | Stufe manuell | Ein | **5.100** |
| 7 | Stufe manuell % | Ein | 5.001 |
| 8 | Stufe höher/niedriger | Ein | 1.007 |
| 9 | Handbetrieb aktiv (0 beendet) | Ein/Aus | 1.012 |
| 10 | Sommer | Ein | 1.001 |
| 11 | Betriebsweise (0 auto / 1 WRG / 2 Zuluft / 3 Abluft) | Ein | 5.010 |
| 12 | Abluftanforderung | Ein | 1.003 |
| 13 | Sperre | Ein | 1.003 |
| 14–20 | rF innen, T innen, rF außen, T außen, CO₂, Luftgüte, Solltemperatur | Ein | 9.007 / 9.001 / 9.008 |
| 21–24 | Führungen aktiv: Temperatur, Feuchte, CO₂, Luftgüte (0 sperrt) | Ein | 1.003 |
| 21 | *oder* Freie Kühlung aktiv (Heizungssperre) | Aus | 1.003 |
| 25 | Raumanforderung Stufe | Aus | **5.100** |
| 26 | Raumanforderung % | Aus | 5.001 |
| 27 / 28 | Betriebsart Status / erweitert | Aus | 20.102 / 5.010 |
| 29 | Betriebsweise Status | Aus | 5.010 |
| 30 | Schutzbetrieb aktiv | Aus | 1.001 |
| 31 | Feuchtevergleich sperrt | Aus | 1.001 |
| 32 / 33 | Absolute Feuchte innen / außen | Aus | 9.029 |
| 34 | Zuluftanforderung | Aus | 1.003 |
| 35 | Intervall aktiv | Aus | 1.001 |
| 36 | Luftgüte als Index | Ein | 5.010 |

### Je Lüfter (Block 24, ab KO 340)

| Nr. | Objekt | Richtung | DPT |
|---|---|---|---|
| 0 | Freigabe | Ein | 1.003 |
| 1 | Suspendieren (Wartung) | Ein | 1.001 |
| 2–5 | Verbund: Stufe, Takt, Lebenszeichen, Betriebsweise | Ein/Aus | 5.100 / 1.012 / 1.001 / 5.010 |
| 6 / 7 | Stufe Status / in % | Aus | **5.100** / 5.001 |
| 8 | Richtung Status (1 = Zuluft) | Aus | 1.001 |
| 9 | Wärmerückgewinnung aktiv | Aus | 1.001 |
| 10 | Ausgangsspannung (mV) | Aus | 9.020 |
| 11 | Volumenstrom, positiv = Zuluft | Aus | 9.009 |
| 12 | Betriebsstunden | Aus | 7.007 |
| 13 | Filterwechsel fällig | Aus | 1.005 |
| 14 | Filter Restlaufzeit | Aus | 5.001 |
| 15 | Filterwechsel quittieren | Ein | 1.016 |
| 16 | Störung | Aus | 1.005 |
| 17 | Fehlercode | Aus | 5.010 |

> Die Stufenobjekte sind **DPT 5.100 (Lüfterstufe)**, nicht 5.010. Wer sie mit
> einem Zählerobjekt verknüpft, bekommt von der ETS eine Warnung.

---

## Fehlercodes

Liegen mehrere an, gewinnt der kleinste.

| Code | Bedeutung | Störungs-KO |
|---|---|---|
| 1 | Freigabe fehlt | ja |
| 2 | Master-Timeout | ja |
| 3 | Konfiguration: Kanalzahl oder Kennlinie passt nicht | ja |
| 4 | DAC nicht erreichbar | ja |
| 5 | Ungültiger Empfangswert | ja |
| 6 | Überwachung ausgesetzt (suspendiert) | nein |
| 7 | Feuchtevergleich sperrt | nein |
| 8 | Sensorwerte fehlen | nein |
| 9 | Schutzbetrieb aktiv | nein |
| 10 | Filterwechsel fällig | nein |
| 11 | Richtungskonflikt im Verbund | nein |

---

## Serielle Konsole

| Befehl | Zeigt |
|---|---|
| `kwl st` | Alle Lüfterkanäle, je eine Zeile |
| `kwl r` | Alle Räume, je eine Zeile |
| `kwl grp` | Verbünde mit Stufe, Richtung, Zykluszeit, Totzeit, Konflikt |
| `kwl f1` | Ein Lüfter ausführlich: Typ, Kanal, Spannung, Kalibrierung, Zähler, Filter |
| `kwl r1` | Ein Raum ausführlich: **warum** die Stufe so ist, wie sie ist |

`kwl r1` ist der schnellste Weg zur Antwort auf „warum läuft der nicht": Es zeigt
den Rang, aus dem die Stufe kommt, welche Messwerte fehlen, ob der
Feuchtevergleich sperrt und in welchem Zustand die Abluftanforderung steht.

---

## Was das Gerät nicht kann

- **Keinen Einschaltwert speichern.** Siehe oben — nach dem Einschalten laufen die
  Lüfter kurz mit voller Drehzahl.
- **Keine Drehzahlrückmeldung.** Das Gerät weiß nicht, ob sich ein Lüfter dreht;
  der angezeigte Volumenstrom ist der Nennwert der Stufe, kein Messwert.
- **Keine Filterüberwachung am Differenzdruck.** Der Filterzähler rechnet Laufzeit
  oder durchgesetzte Luftmenge hoch.
- **Kein Ersatz für einen verdrahteten Sicherheitskontakt** an einer Feuerstätte.
