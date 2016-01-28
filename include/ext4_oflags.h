/*
 * Copyright (c) 2015 Grzegorz Kostka (kostka.grzegorz@gmail.com)
 * Copyright (c) 2015 Kaho Ng (ngkaho1234@gmail.com)
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
 * @file  ext4_oflags.h
 * @brief File opening & seeking flags.
 */
#ifndef EXT4_OFLAGS_H_
#define EXT4_OFLAGS_H_

#ifdef __cplusplus
extern "C" {
#endif

/********************************FILE OPEN FLAGS*****************************/

#if CONFIG_HAVE_OWN_OFLAGS

 #ifndef O_RDONLY
 #define O_RDONLY 00
 #endif

 #ifndef O_WRONLY
 #define O_WRONLY 01
 #endif

 #ifndef O_RDWR
 #define O_RDWR 02
 #endif

 #ifndef O_CREAT
 #define O_CREAT 0100
 #endif

 #ifndef O_EXCL
 #define O_EXCL 0200
 #endif

 #ifndef O_TRUNC
 #define O_TRUNC 01000
 #endif

 #ifndef O_APPEND
 #define O_APPEND 02000
 #endif

/********************************FILE SEEK FLAGS*****************************/

 #ifndef SEEK_SET
 #define SEEK_SET 0
 #endif

 #ifndef SEEK_CUR
 #define SEEK_CUR 1
 #endif

 #ifndef SEEK_END
 #define SEEK_END 2
 #endif

#else
 #include <unistd.h>
 #include <fcntl.h>
#endif

#ifdef __cplusplus
}
#endif

#endif /* EXT4_OFLAGS_H_ */

/**
 * @}
 */
