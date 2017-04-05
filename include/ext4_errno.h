/*
 * Copyright (c) 2013 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 * - The name of the author may not be used to endorse or promote products
 *   derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/** @addtogroup lwext4
 * @{
 */
/**
 * @file  ext4_errno.h
 * @brief Error codes.
 */
#ifndef EXT4_ERRNO_H_
#define EXT4_ERRNO_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <ext4_config.h>

#if !CONFIG_HAVE_OWN_ERRNO
#include <errno.h>
#else
#define EPERM 1      /* Operation not permitted */
#define ENOENT 2     /* No such file or directory */
#define EIO 5        /* I/O error */
#define ENXIO 6      /* No such device or address */
#define E2BIG 7      /* Argument list too long */
#define ENOMEM 12    /* Out of memory */
#define EACCES 13    /* Permission denied */
#define EFAULT 14    /* Bad address */
#define EEXIST 17    /* File exists */
#define ENODEV 19    /* No such device */
#define ENOTDIR 20   /* Not a directory */
#define EISDIR 21    /* Is a directory */
#define EINVAL 22    /* Invalid argument */
#define EFBIG 27     /* File too large */
#define ENOSPC 28    /* No space left on device */
#define EROFS 30     /* Read-only file system */
#define EMLINK 31    /* Too many links */
#define ERANGE 34    /* Math result not representable */
#define ENOTEMPTY 39 /* Directory not empty */
#define ENODATA 61   /* No data available */
#define ENOTSUP 95   /* Not supported */
#endif

#ifndef ENODATA
 #ifdef ENOATTR
 #define ENODATA ENOATTR
 #else
 #define ENODATA 61
 #endif
#endif

#ifndef ENOTSUP
#define ENOTSUP 95
#endif

#ifndef EOK
#define EOK 0
#endif

#ifdef __cplusplus
}
#endif

#endif /* EXT4_ERRNO_H_ */

/**
 * @}
 */
