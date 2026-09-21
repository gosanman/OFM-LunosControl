### Wartezeit nach Busspannungswiederkehr

Wartezeit, bevor nach Busspannungswiederkehr die
ersten Sollwerte auf die Ausgänge geschrieben werden.

Die Pause gibt den Sensoren und Bedienstellen im Haus Zeit, ihre Werte zu senden. Ohne sie fährt
das Gerät zunächst mit den Vorgaben der Betriebsart los und korrigiert sich Sekunden später — bei
mehreren Geräten zugleich erzeugt das unnötigen Busverkehr.

Der Bereichsbefehl an die DACs und der sichere Zustand 5,00 V werden **vor** dieser Wartezeit
geschrieben, nicht danach.
