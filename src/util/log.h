/* vi: set ts=2:
+-------------------+  Christian Schlittchen <corwin@amber.kn-bremen.de>
|                   |  Enno Rehling <enno@eressea.de>
| Eressea PBEM host |  Katja Zedel <katze@felidae.kn-bremen.de>
| (c) 1998 - 2003   |  Henning Peters <faroul@beyond.kn-bremen.de>
|                   |  Ingo Wilken <Ingo.Wilken@informatik.uni-oldenburg.de>
+-------------------+  Stefan Reich <reich@halbling.de>

This program may not be used, modified or distributed 
without prior permission by the authors of Eressea.
*/
#ifndef H_UTIL_LOG
#define H_UTIL_LOG
#ifdef __cplusplus
extern "C" {
#endif

  extern void log_open(const char *filename);
  extern void log_printf(const char *str, ...);
  extern void log_puts(const char *str);
  extern void log_close(void);
  extern void log_flush(void);
  extern void log_stdio(FILE * io, const char *format, ...);

#define log_warning(x) _log_warn x
#define log_error(x) _log_error x
#define log_info(x) _log_info x

  /* use macros above instead of these: */
  extern void _log_warn(const char *format, ...);
  extern void _log_error(const char *format, ...);
  extern void _log_info(unsigned int flag, const char *format, ...);

#define LOG_FLUSH      0x01
#define LOG_CPWARNING  0x02
#define LOG_CPERROR    0x04
#define LOG_INFO1      0x08
#define LOG_INFO2      0x10
#define LOG_INFO3      0x20

  extern int log_flags;
#ifdef __cplusplus
}
#endif
#endif
