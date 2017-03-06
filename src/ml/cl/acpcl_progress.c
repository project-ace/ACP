/*
 * ACP Middle Layer: Communication Library
 * 
 * Copyright (c) 2014-2014 Kyushu University
 * Copyright (c) 2014      Institute of Systems, Information Technologies
 *                         and Nanotechnologies 2014
 * Copyright (c) 2014      FUJITSU LIMITED
 *
 * This software is released under the BSD License, see LICENSE.
 *
 * Note:
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <acp.h>
#include "acpcl_progress.h"

extern void iacpcl_progress_ch(void);

extern void iacpcl_progress_segbuf(void);

void iacpcl_progress(void)
{
    iacpcl_progress_ch();
    iacpcl_progress_segbuf();
}
