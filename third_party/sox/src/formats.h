/* WaveToAmiga: reduzierte Liste der statisch eingebundenen SoX-Format-Handler.
 *
 * Ersetzt die originale SoX-Datei "formats.h" (LGPL v2.1 oder neuer, (c) 2006-9 Chris Bagwell
 * and SoX contributors). Nur die für WaveToAmiga benötigten Handler sind eingetragen.
 */

  FORMAT(svx)       /* SoX-Original: IFF 8SVX (8 Bit, unkomprimiert)                    */
  FORMAT(aiff)      /* SoX-Original: AIFF                                               */
  FORMAT(maud)      /* SoX-Original: IFF MAUD                                           */
  FORMAT(svxfib)    /* WaveToAmiga:  IFF 8SVX mit Fibonacci-Delta-Kompression           */
  FORMAT(sv16)      /* WaveToAmiga:  IFF 16SV (16 Bit)                                  */
