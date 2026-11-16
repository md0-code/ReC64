/* ReC64 - everything that depends on the operating system, behind one
 * portable header (PLAN.md section 3). One implementation per OS:
 * host_win32.c (built and tested) and host_posix.c (Linux; compiled from
 * the release phase on). Callers include only this header, never an OS
 * header.  */
#ifndef RC_HOST_H
#define RC_HOST_H

#include <stddef.h>
#include <stdint.h>

#define RC_HOST_PATH 1024

/* --- process ----------------------------------------------------------------- */

/* Makes the program's messages reach a console: the one it was started
 * from, or the pipes a script gave it; UTF-8 output. Returns 1 when
 * messages go nowhere visible, so a fatal one should also be shown with
 * rc_host_alert. */
int rc_host_console_init(void);
/* A fatal message for a user who started the program without a console. */
void rc_host_alert(const char *title, const char *text);
/* The folder of the running executable, with its separator; 0 when unknown. */
int rc_host_program_folder(char *out, size_t capacity);
/* Sleeps for about `us` microseconds, to within a fraction of a millisecond
 * (a high-resolution timer: the system's coarse default, up to 15 ms on a
 * hidden or covered window, would hold the desktop loop's pictures to half
 * their rate). May return early or late; the caller reads its clock. */
void rc_host_sleep_us(uint32_t us);

/* The user's locale as a name ("ro-RO" on Windows; on POSIX LC_ALL, LC_MESSAGES
 * or LANG, "ro_RO.UTF-8"); 0 when the system does not say. */
int rc_host_user_locale(char *out, size_t capacity);

/* The local date and time now (the wall clock, for what a user reads: when a
 * state was saved); 0 when the system does not say. */
typedef struct rc_host_time {
    uint16_t year;
    uint8_t month, day, hour, minute, second;   /* month and day from 1 */
} rc_host_time;
int rc_host_local_time(rc_host_time *t);

/* --- files ------------------------------------------------------------------- */

/* The folder separator ('\\' or '/'). */
char rc_host_separator(void);
/* Calls fn for each regular file of `folder` (a path with or without its
 * separator), in no particular order; returns 0 when the folder cannot be
 * read (missing included). fn returns 0 to stop. */
typedef int (*rc_host_file_fn)(void *ctx, const char *name);
int rc_host_list_files(const char *folder, rc_host_file_fn fn, void *ctx);
/* Replaces `path` by `temporary` in one step (a saved file is whole or
 * not there at all). */
int rc_host_replace_file(const char *temporary, const char *path);
/* Folders and files for the setup screen's file picker. fn is called for
 * each folder (size 0) and regular file of `folder` ("." and ".." are not
 * listed, hidden and system entries are), in no particular order; returns 0
 * when the folder cannot be read. fn returns 0 to stop. */
typedef int (*rc_host_entry_fn)(void *ctx, const char *name, int is_folder, uint64_t size);
int rc_host_list_folder(const char *folder, rc_host_entry_fn fn, void *ctx);
/* The roots of the file system, each as a folder name with its separator:
 * the drives on Windows ("C:\"), "/" elsewhere. 0 when none can be listed. */
int rc_host_list_roots(rc_host_entry_fn fn, void *ctx);
/* The size of a regular file; 0 when there is none (a folder included). */
int rc_host_file_size(const char *path, uint64_t *size);
/* 1 when `path` is a folder. */
int rc_host_folder_exists(const char *path);
/* A path made absolute, with "." and ".." resolved; 0 when it does not fit. */
int rc_host_full_path(const char *path, char *out, size_t capacity);
/* 1 when the two paths name the same folder (case and spelling as the
 * file system sees them). */
int rc_host_same_folder(const char *a, const char *b);
/* Makes a folder and the folders above it that are missing; 1 when it is
 * there afterwards. */
int rc_host_make_folders(const char *path);
/* Removes a file; 1 when it is gone (0 when it cannot be removed; a file that
 * was not there counts as gone). */
int rc_host_delete_file(const char *path);

/* --- byte streams for the serial ports ---------------------------------------- */

/* A host end of a guest serial port: a serial device (Windows "COM3",
 * POSIX "/dev/ttyUSB0"), a local pipe (Windows "\\.\pipe\NAME", POSIX a Unix
 * socket path) or a TCP connection. A pipe served by someone else is joined;
 * otherwise ReC64 serves it and waits for a client without blocking.
 * TCP_CLIENT ("HOST:PORT") connects to the address, again every second while
 * nobody answers or after the other side went away; TCP_LISTEN ("PORT" or
 * "HOST:PORT", on the local interface 127.0.0.1 unless a HOST such as
 * 0.0.0.0 is given; PORT 0 takes any free port) waits for one client at a
 * time. IPv4 only. Bytes written while no one is connected are gone. */
typedef struct rc_host_stream rc_host_stream;
enum { RC_STREAM_SERIAL, RC_STREAM_PIPE, RC_STREAM_TCP_CLIENT, RC_STREAM_TCP_LISTEN };
rc_host_stream *rc_host_stream_open(int kind, const char *path, char *error, size_t capacity);
/* Non-blocking: the bytes there now (0 none), -1 when the other end went
 * away (a served pipe, a listener and a TCP client then wait for the next
 * connection). */
long rc_host_stream_read(rc_host_stream *s, uint8_t *data, size_t capacity);
/* The bytes written (fewer when the line is busy), -1 on a broken end. */
long rc_host_stream_write(rc_host_stream *s, const uint8_t *data, size_t count);
/* The modem inputs: a device's DCD, CTS, DSR and RI; a pipe's and a TCP
 * end's are its connection. 0 when they cannot be read. */
int rc_host_stream_modem(rc_host_stream *s, int *dcd, int *cts, int *dsr, int *ri);
/* The guest's DTR and RTS outputs (a device's pins; nothing on a pipe). */
void rc_host_stream_outputs(rc_host_stream *s, int dtr, int rts);
/* The port a TCP listener is bound to (0 for any other stream). */
unsigned rc_host_stream_local_port(const rc_host_stream *s);
void rc_host_stream_close(rc_host_stream *s);

/* The serial ports this computer has (Windows: the names the drivers
 * registered, "COM3"; POSIX: /dev/ttyUSB*, ttyACM* and the UARTs that
 * exist), in order; fn gets each name and returns 0 to stop. Returns how
 * many there are (0: none). */
typedef int (*rc_host_name_fn)(void *ctx, const char *name);
unsigned rc_host_list_serial(rc_host_name_fn fn, void *ctx);

/* --- whole files (disk images) ------------------------------------------------ */

/* 1 when `path` names an existing file or folder. */
int rc_host_file_exists(const char *path);
/* Reads the whole of a file of at most `limit` bytes into a buffer from
 * malloc (the caller frees it). *writable is 0 when the file is read-only.
 * 0 with the reason in `error`. */
int rc_host_read_file(const char *path, size_t limit, uint8_t **data, size_t *size, int *writable, char *error, size_t capacity);
/* Makes `data` the whole content of `path`: written to a temporary file
 * beside it, flushed to the disk, then put in place in one step, so a crash
 * leaves the old file or the new one. 0 with the reason in `error`. */
int rc_host_write_file(const char *path, const uint8_t *data, size_t size, char *error, size_t capacity);

/* --- files read and written in place (fixed disk images) ------------------------ */

/* A large file used a sector at a time: never read whole, written where it
 * changes. */
typedef struct rc_host_file rc_host_file;
/* Opens an existing file for reading and, when it allows it, writing
 * (*writable says which). NULL with the reason in `error`. */
rc_host_file *rc_host_file_open(const char *path, int *writable, char *error, size_t capacity);
/* A new file of `size` bytes that read as zero, sparse where the file system
 * can (only the parts written take room). An existing file is replaced only
 * with `overwrite`. 0 with the reason in `error`. */
int rc_host_file_create(const char *path, uint64_t size, int overwrite, char *error, size_t capacity);
/* `length` bytes at `offset`; 1 when all of them moved. */
int rc_host_file_read_at(rc_host_file *f, uint64_t offset, void *data, size_t length);
int rc_host_file_write_at(rc_host_file *f, uint64_t offset, const void *data, size_t length);
/* What was written reaches the disk; 0 when it cannot. */
int rc_host_file_flush(rc_host_file *f);
uint64_t rc_host_file_length(const rc_host_file *f);
void rc_host_file_close(rc_host_file *f);

#endif /* RC_HOST_H */
