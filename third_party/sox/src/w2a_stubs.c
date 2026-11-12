/* WaveToAmiga: libsox-Teilmenge ohne Effekte-Kette. sox_init()/sox_quit() rufen die Effekt-Verwaltung
 * auf; da WaveToAmiga keine SoX-Effekte benutzt, genügen leere Platzhalter (nicht Teil der SoX-Quellen).
 * Copyright (c) 2026 Michael Bergmann - LGPL v2.1+ wie libsox. */
#include "sox_i.h"

int lsx_effects_init(void) { return SOX_SUCCESS; }
int lsx_effects_quit(void) { return SOX_SUCCESS; }
