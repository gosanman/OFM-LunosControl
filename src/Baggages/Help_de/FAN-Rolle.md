### Rolle

Bestimmt, woher dieser Verbund seine Stufe und seinen Takt bezieht.

* **intern** — der Verbund bildet beides selbst aus den Anforderungen seiner Räume. Der Normalfall
  für Lüfter, die alle an dieser Platine hängen.
* **Master** — wie intern, sendet Stufe, Takt, Betriebsweise und ein Lebenszeichen zusätzlich auf
  den Bus, damit Geräte in anderen Verteilungen mitlaufen können.
* **Slave** — der Verbund rechnet nicht selbst, sondern folgt einem externen Master über die
  Gruppen-Kommunikationsobjekte.

Bei mehreren Lüftern im selben externen Verbund sendet nur der erste.
