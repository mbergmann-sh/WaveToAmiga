/* WaveToAmiga: IFF-Handler für libsox im Stil von 8svx.c (SoX), Erweiterungen:
 *
 *   svxfib  IFF 8SVX mit Fibonacci-Delta-Kompression (sCompression = 1, EA IFF 8SVX Appendix C)
 *   sv16    IFF 16SV (16 Bit, Big-Endian, Aufbau wie 8SVX: VHDR/CHAN/BODY)
 *
 * Nur Schreiben (WAV -> Amiga). Der Aufbau der Handler (Zwischendateien je Kanal, Header
 * erst in stopwrite) folgt dem SoX-Handler 8svx.c ("Amiga 8SVX format handler: W V Neisius,
 * February 1992"), von dem diese Datei abgeleitet ist.
 *
 * Lizenz: GNU Lesser General Public License, Version 2.1 oder (nach Wahl) neuer - wie libsox.
 * Copyright (c) 2026 Michael Bergmann (Ergänzungen)
 *
 * This library is free software; you can redistribute it and/or modify it under the terms of
 * the GNU Lesser General Public License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version. It is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 */

#include "sox_i.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CH 4

typedef struct {
  uint32_t frames;          /* Samples je Kanal */
  FILE * tmp[MAX_CH];       /* Zwischendatei je Kanal (8 Bit bzw. 16 Bit Big-Endian) */
} priv_t;

/* Fibonacci-Delta-Tabelle laut EA IFF 8SVX, Appendix C */
static const int fib_delta[16] = {-34, -21, -13, -8, -5, -3, -2, -1, 0, 1, 2, 3, 5, 8, 13, 21};

/*----------------------------------------------------------------------------*/
static int startwrite_common(sox_format_t * ft)
{
  priv_t * p = (priv_t *) ft->priv;
  size_t i;

  if (ft->signal.channels < 1 || ft->signal.channels > MAX_CH) {
    lsx_fail_errno(ft, SOX_EFMT, "IFF 8SVX/16SV unterstützt 1 bis %d Kanäle", MAX_CH);
    return SOX_EOF;
  }
  for (i = 0; i < ft->signal.channels; i++) {
    if ((p->tmp[i] = lsx_tmpfile()) == NULL) {
      lsx_fail_errno(ft, errno, "Can't open channel output file");
      return SOX_EOF;
    }
  }
  p->frames = 0;
  return SOX_SUCCESS;
}

static size_t write_samples8(sox_format_t * ft, const sox_sample_t * buf, size_t len)
{
  priv_t * p = (priv_t *) ft->priv;
  SOX_SAMPLE_LOCALS;
  size_t done = 0, i;

  while (done < len) {
    for (i = 0; i < ft->signal.channels; i++)
      putc((unsigned char) SOX_SAMPLE_TO_SIGNED_8BIT(*buf++, ft->clips), p->tmp[i]);
    done += ft->signal.channels;
    p->frames++;
  }
  return done;
}

static size_t write_samples16(sox_format_t * ft, const sox_sample_t * buf, size_t len)
{
  priv_t * p = (priv_t *) ft->priv;
  SOX_SAMPLE_LOCALS;
  size_t done = 0, i;

  while (done < len) {
    for (i = 0; i < ft->signal.channels; i++) {
      int16_t v = SOX_SAMPLE_TO_SIGNED_16BIT(*buf++, ft->clips);
      putc((unsigned char) ((v >> 8) & 0xff), p->tmp[i]);   /* Big-Endian: High-Byte zuerst */
      putc((unsigned char) (v & 0xff), p->tmp[i]);
    }
    done += ft->signal.channels;
    p->frames++;
  }
  return done;
}

/* Gemeinsamer Header: FORM <form> / VHDR / ANNO / CHAN / BODY (body_bytes = Länge von BODY) */
static void write_header(sox_format_t * ft, char const * form, uint32_t frames,
                         uint32_t body_bytes, int compression)
{
  static char const anno[] = "File created by WaveToAmiga (libsox)    "; /* 40 Zeichen */
  uint32_t anno_len = 40;
  uint32_t body_padded = body_bytes + (body_bytes & 1u);
  /* FORM-Größe = alles hinter dem FORM-Header: "xxxx"(4) + VHDR(8+20) + ANNO(8+40) + CHAN(8+4) + BODY(8+body) */
  uint32_t formsize = 4 + 28 + (8 + anno_len) + 12 + 8 + body_padded;

  lsx_writes(ft, "FORM");
  lsx_writedw(ft, formsize);
  lsx_writes(ft, form);

  lsx_writes(ft, "VHDR");
  lsx_writedw(ft, 20);
  lsx_writedw(ft, frames);                 /* oneShotHiSamples (Samples je Kanal, unkomprimiert gezählt) */
  lsx_writedw(ft, 0);                      /* repeatHiSamples */
  lsx_writedw(ft, 0);                      /* samplesPerHiCycle */
  lsx_writew(ft, min(65535, (unsigned)(ft->signal.rate + .5)));  /* samplesPerSec */
  lsx_writeb(ft, 1);                       /* ctOctave */
  lsx_writeb(ft, (unsigned char) compression); /* sCompression: 0 = keine, 1 = Fibonacci-Delta */
  lsx_writew(ft, 1); lsx_writew(ft, 0);    /* volume = Unity (0x00010000) */

  lsx_writes(ft, "ANNO");
  lsx_writedw(ft, anno_len);
  lsx_writebuf(ft, anno, anno_len);

  lsx_writes(ft, "CHAN");
  lsx_writedw(ft, 4);
  lsx_writedw(ft, (ft->signal.channels == 2) ? 6u : (ft->signal.channels == 4) ? 15u : 2u);

  lsx_writes(ft, "BODY");
  lsx_writedw(ft, body_bytes);
}

/*------------------------------- 16SV ---------------------------------------*/
static int stopwrite16(sox_format_t * ft)
{
  priv_t * p = (priv_t *) ft->priv;
  size_t i, len;
  char buf[512];
  uint32_t body = p->frames * 2u * ft->signal.channels;

  write_header(ft, "16SV", p->frames, body, 0);

  for (i = 0; i < ft->signal.channels; i++) {
    if (fseeko(p->tmp[i], (off_t) 0, 0)) {
      lsx_fail_errno(ft, errno, "Can't rewind channel output file %lu", (unsigned long) i);
      return SOX_EOF;
    }
    while (!feof(p->tmp[i])) {
      len = fread(buf, 1, sizeof buf, p->tmp[i]);
      if (lsx_writebuf(ft, buf, len) != len) {
        lsx_fail_errno(ft, errno, "Can't write channel output file %lu", (unsigned long) i);
        return SOX_EOF;
      }
    }
    fclose(p->tmp[i]);
  }
  if (body & 1u) lsx_writeb(ft, '\0');
  return SOX_SUCCESS;
}

/*------------------------- 8SVX Fibonacci-Delta -----------------------------*/
/* Komprimiert n 8-Bit-Samples (vorzeichenbehaftet) eines Kanals. Ausgabe: Pad-Byte, Startwert,
 * danach je zwei 4-Bit-Codes pro Byte (High-Nibble zuerst). Der Dekoder addiert Delta auf den
 * Startwert; der Encoder wählt je Sample den Code, dessen Ergebnis dem Zielwert am nächsten liegt
 * (Fehlerrückkopplung über den tatsächlich dekodierten Wert, Bereich bleibt -128..127). */
static int fib_pick(int pred, int target)
{
  int best = 8, besterr = 1 << 30, c;
  for (c = 0; c < 16; c++) {
    int v = pred + fib_delta[c];
    int err;
    if (v < -128 || v > 127) continue;     /* nie über den 8-Bit-Bereich hinaus (kein Wrap im Dekoder) */
    err = v > target ? v - target : target - v;
    if (err < besterr) { besterr = err; best = c; }
  }
  return best;
}

static int stopwrite_fib(sox_format_t * ft)
{
  priv_t * p = (priv_t *) ft->priv;
  size_t ch;
  uint32_t n = p->frames;
  uint32_t pairs = (n + 1) / 2;                       /* ungerade Anzahl -> letztes Sample wird wiederholt */
  uint32_t per_ch = 2 + pairs;
  uint32_t body = per_ch * (uint32_t) ft->signal.channels;

  write_header(ft, "8SVX", n, body, 1);

  for (ch = 0; ch < ft->signal.channels; ch++) {
    int pred, first = 0, have_first = 0;
    uint32_t i;
    int hi = 0, nib_pending = 0;

    if (fseeko(p->tmp[ch], (off_t) 0, 0)) {
      lsx_fail_errno(ft, errno, "Can't rewind channel output file %lu", (unsigned long) ch);
      return SOX_EOF;
    }
    /* erstes Sample lesen, um es als Startwert zu verwenden (erstes Delta wird dann 0) */
    {
      int c0 = getc(p->tmp[ch]);
      if (c0 != EOF) { first = (signed char) c0; have_first = 1; }
      if (have_first) fseeko(p->tmp[ch], (off_t) 0, 0);
    }
    lsx_writeb(ft, 0);                                  /* Pad-Byte */
    lsx_writeb(ft, (unsigned) (first & 0xff));          /* Startwert */
    pred = first;

    for (i = 0; i < pairs * 2; i++) {
      int target, c;
      int raw = (i < n) ? getc(p->tmp[ch]) : EOF;
      if (raw != EOF) target = (signed char) raw; else target = pred;   /* Auffüllen: Wert halten */
      c = fib_pick(pred, target);
      pred += fib_delta[c];
      if (!nib_pending) { hi = c; nib_pending = 1; }
      else { lsx_writeb(ft, (unsigned) ((hi << 4) | c)); nib_pending = 0; }
    }
    fclose(p->tmp[ch]);
  }
  if (body & 1u) lsx_writeb(ft, '\0');
  return SOX_SUCCESS;
}

LSX_FORMAT_HANDLER(svxfib)
{
  static char const * const names[] = {"svxfib", "8svxfib", NULL};
  static unsigned const write_encodings[] = {SOX_ENCODING_SIGN2, 8, 0, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "Amiga 8SVX with Fibonacci-delta compression",
    names, SOX_FILE_BIG_END|SOX_FILE_MONO|SOX_FILE_STEREO|SOX_FILE_QUAD,
    NULL, NULL, NULL,
    startwrite_common, write_samples8, stopwrite_fib,
    NULL, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}

LSX_FORMAT_HANDLER(sv16)
{
  static char const * const names[] = {"16sv", "sv16", NULL};
  static unsigned const write_encodings[] = {SOX_ENCODING_SIGN2, 16, 0, 0};
  static sox_format_handler_t const handler = {SOX_LIB_VERSION_CODE,
    "Amiga 16SV (16-bit IFF sampled voice)",
    names, SOX_FILE_BIG_END|SOX_FILE_MONO|SOX_FILE_STEREO|SOX_FILE_QUAD,
    NULL, NULL, NULL,
    startwrite_common, write_samples16, stopwrite16,
    NULL, write_encodings, NULL, sizeof(priv_t)
  };
  return &handler;
}
