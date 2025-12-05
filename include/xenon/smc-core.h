// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef _XENON_SMC_CORE_H
#define _XENON_SMC_CORE_H

int xenon_smc_ready(void);
int xenon_smc_message(void *msg);
int xenon_smc_message_wait(void *msg);
void xenon_smc_restart(void);
void xenon_smc_power_off(void);
void xenon_smc_halt(void);

#endif

