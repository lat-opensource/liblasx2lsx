/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LASX_ENTRY_COMPAT_H
#define LASX_ENTRY_COMPAT_H

#include <linux/version.h>

/* Vendor LoongArch 4.19 and 5.4 use ret_from_exception. The C wrappers
 * and assembly return must agree on which side handles return-to-user work.
 * Keep this cutoff separate from the kallsyms export and MM API versions.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 17, 0)
#define LASX_USE_IRQENTRY 1
#else
#define LASX_USE_IRQENTRY 0
#endif

#endif
