/*
 * errno_to_1541.h
 *
 * Mapping from POSIX errno values to Commodore 1541 disk drive error codes.
 *
 * The 1541 error channel returns a string of the form:
 *   NN,MESSAGE,TT,SS\r
 * where NN is the two-digit error code, TT is track and SS is sector
 * (both 00 for non-positional errors).
 *
 * This table covers file-related errno values only.
 * Unmapped errno values should fall back to a generic I/O error (code 20).
 */

#ifndef ERRNO_TO_1541_H
#define ERRNO_TO_1541_H

#include <errno.h>
#include <stddef.h>

typedef struct {
    int               err_no;       /* POSIX errno value                  */
    const char       *err_macro;    /* errno macro name, for readability  */
    int               cbm_code;     /* Commodore 1541 error channel code  */
    const char       *cbm_message;  /* Corresponding 1541 error message   */
    const char       *notes;        /* Rationale for the mapping          */
} errno_cbm_pair_t;

static const errno_cbm_pair_t errno_to_1541_table[] = {

    /* ------------------------------------------------------------------ */
    /* No error                                                             */
    /* ------------------------------------------------------------------ */
    {  0,        "-",            0,  "OK",
       "Success." },

    /* ------------------------------------------------------------------ */
    /* File / directory existence                                           */
    /* ------------------------------------------------------------------ */
    {  ENOENT,   "ENOENT",      62,  "FILE NOT FOUND",
       "File does not exist on disk." },

    {  EEXIST,   "EEXIST",      63,  "FILE EXISTS",
       "Cannot overwrite without scratching first." },

    {  ENOTEMPTY,"ENOTEMPTY",   63,  "FILE EXISTS",
       "1541 has no subdirectories; closest is 'file exists'." },

    /* ------------------------------------------------------------------ */
    /* File type / format mismatches                                        */
    /* ------------------------------------------------------------------ */
    {  EISDIR,   "EISDIR",      64,  "FILE TYPE MISMATCH",
       "1541 has no directories; type mismatch is the best fit." },

    {  ENOTDIR,  "ENOTDIR",     64,  "FILE TYPE MISMATCH",
       "Path component is not a directory - type mismatch." },

    {  EBADF,    "EBADF",       61,  "FILE NOT OPEN",
       "Bad file descriptor: file was never opened." },

    /* ------------------------------------------------------------------ */
    /* Access / permissions                                                 */
    /* ------------------------------------------------------------------ */
    {  EROFS,    "EROFS",       26,  "WRITE PROTECT ON",
       "Read-only filesystem - physical write-protect tab engaged." },

    {  EACCES,   "EACCES",      26,  "WRITE PROTECT ON",
       "Permission denied - closest 1541 concept is write-protect." },

    {  EPERM,    "EPERM",       26,  "WRITE PROTECT ON",
       "Operation not permitted - mapped to write-protect." },

    /* ------------------------------------------------------------------ */
    /* Space / size limits                                                  */
    /* ------------------------------------------------------------------ */
    {  ENOSPC,   "ENOSPC",      72,  "DISK FULL",
       "No space left on device." },

    {  EFBIG,    "EFBIG",       52,  "FILE TOO LARGE",
       "File too large - REL files have a maximum record count." },

    {  EDQUOT,   "EDQUOT",      72,  "DISK FULL",
       "Quota exceeded - no quota concept on 1541, disk full is closest." },

    /* ------------------------------------------------------------------ */
    /* I/O and hardware errors                                              */
    /* ------------------------------------------------------------------ */
    {  EIO,      "EIO",         20,  "READ ERROR (block header not found)",
       "Generic I/O error - default to read error code 20." },

    {  EILSEQ,   "EILSEQ",      24,  "READ ERROR (byte decoding error)",
       "Illegal byte sequence - maps to GCR byte decoding error." },

    {  EDEADLK,  "EDEADLK",    71,  "DIRECTORY ERROR",
       "Resource deadlock - closest is internal directory corruption." },

    /* ------------------------------------------------------------------ */
    /* Resource / descriptor limits                                         */
    /* ------------------------------------------------------------------ */
    {  EMFILE,   "EMFILE",      70,  "NO CHANNEL",
       "Too many open files - 1541 supports only 5 channels." },

    {  ENFILE,   "ENFILE",      70,  "NO CHANNEL",
       "File table overflow - same as EMFILE for 1541 purposes." },

    {  ENOLCK,   "ENOLCK",      70,  "NO CHANNEL",
       "No record locks available - no channel available." },

    /* ------------------------------------------------------------------ */
    /* Naming errors                                                        */
    /* ------------------------------------------------------------------ */
    {  ENAMETOOLONG, "ENAMETOOLONG", 32, "SYNTAX ERROR (long line)",
       "Filename exceeds 16 characters (1541 limit) or command line too long." },

    {  EINVAL,   "EINVAL",      33,  "SYNTAX ERROR (invalid filename)",
       "Invalid argument - bad filename characters or illegal command." },

    /* ------------------------------------------------------------------ */
    /* Positional / record errors (REL files)                               */
    /* ------------------------------------------------------------------ */
    {  ESPIPE,   "ESPIPE",      50,  "RECORD NOT PRESENT",
       "Illegal seek - REL file record does not exist at that position." },

    {  EOVERFLOW,"EOVERFLOW",   51,  "OVERFLOW IN RECORD",
       "Value too large - data overflows a REL file record." },

    /* ------------------------------------------------------------------ */
    /* Device / media errors                                                */
    /* ------------------------------------------------------------------ */
    {  ENODEV,   "ENODEV",      74,  "DRIVE NOT READY",
       "No such device - no disk inserted or drive door open." },

    {  ENXIO,    "ENXIO",       74,  "DRIVE NOT READY",
       "No such device or address - drive not ready." },

    {  EMEDIUMTYPE, "EMEDIUMTYPE", 29, "DISK ID MISMATCH",
       "Wrong medium type - disk swapped without re-initialising." },

    {  ENOMEDIUM,   "ENOMEDIUM",  74, "DRIVE NOT READY",
       "No medium found - no disk in drive." },

    /* ------------------------------------------------------------------ */
    /* Concurrency / state errors                                           */
    /* ------------------------------------------------------------------ */
    {  EALREADY, "EALREADY",    60,  "WRITE FILE OPEN",
       "Operation already in progress - file is already open for write." },

    {  EBUSY,    "EBUSY",       60,  "WRITE FILE OPEN",
       "Device or resource busy - channel already in use." },

    {  EAGAIN,   "EAGAIN",      70,  "NO CHANNEL",
       "Resource temporarily unavailable - no free channel right now." },

};

#define ERRNO_TO_1541_TABLE_SIZE \
    (sizeof(errno_to_1541_table) / sizeof(errno_to_1541_table[0]))

/*
 * errno_to_cbm_code() - look up the 1541 error code for a given errno.
 *
 * Returns the CBM error code, or 20 (READ ERROR) as a generic fallback
 * for any errno not present in the table.
 */
static inline int errno_to_cbm_code(int err_no)
{
    for (size_t i = 0; i < ERRNO_TO_1541_TABLE_SIZE; i++) {
        if (errno_to_1541_table[i].err_no == err_no)
            return errno_to_1541_table[i].cbm_code;
    }
    return 20; /* READ ERROR - generic fallback */
}

static inline int errno_to_cbm_index(int err_no)
{
    for (size_t i = 0; i < ERRNO_TO_1541_TABLE_SIZE; i++) {
        if (errno_to_1541_table[i].err_no == err_no)
            return i;
    }
    return 13; /* 20 READ ERROR - generic fallback */
}
#endif /* ERRNO_TO_1541_H */
