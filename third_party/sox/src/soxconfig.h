/* WaveToAmiga: feste Konfiguration für den eingebundenen SoX-Kern (libsox-Teilmenge),
 * ersetzt die von ./configure erzeugte "soxconfig.h". Nicht Teil der SoX-Originalquellen. */
#ifndef SOXCONFIG_H
#define SOXCONFIG_H

#define PACKAGE_NAME "sox"
#define PACKAGE_VERSION "14.4.2"
#define STDC_HEADERS 1
#define HAVE_STRING_H 1
#define HAVE_STDLIB_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_VSNPRINTF 1

#if defined(_WIN32)
# define HAVE_IO_H 1
# if defined(_MSC_VER)
#  define HAVE__FSEEKI64 1
#  define HAVE_STRCASECMP 0
#  undef HAVE_STRCASECMP
# endif
#else
# define HAVE_UNISTD_H 1
# define HAVE_FSEEKO 1
# define HAVE_STRCASECMP 1
# define HAVE_MKSTEMP 1
#endif

/* bewusst NICHT definiert: HAVE_POPEN, HAVE_FMEMOPEN, HAVE_LIBLTDL, HAVE_MAGIC, HAVE_BYTESWAP_H,
 * HAVE_OPENMP, WORDS_BIGENDIAN (Zielplattformen sind Little-Endian), alle externen Codec-Bibliotheken. */

#endif /* SOXCONFIG_H */
