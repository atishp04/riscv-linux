/*
 * Copyright (C) 2012 Regents of the University of California
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation, version 2.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 */

#ifndef _ASM_RISCV_SMP_H
#define _ASM_RISCV_SMP_H

/* This both needs asm-offsets.h and is used when generating it. */
#ifndef GENERATING_ASM_OFFSETS
#include <asm/asm-offsets.h>
#endif

#include <linux/cpumask.h>
#include <linux/irqreturn.h>

/*
 * Mapping between linux logical cpu index and hartid.
 */
extern u64 __cpu_logical_map[NR_CPUS];
#define cpu_logical_map(cpu)    __cpu_logical_map[cpu]

struct cpu_operations {
	const char	*name;
	int		(*cpu_init)(unsigned int);
	int		(*cpu_prepare)(unsigned int);
	int		(*cpu_boot)(unsigned int, struct task_struct *);
#ifdef CONFIG_HOTPLUG_CPU
	int		(*cpu_disable)(unsigned int cpu);
	void		(*cpu_die)(unsigned int cpu);
#endif
};
extern struct cpu_operations cpu_ops;
void smp_set_cpu_ops(const struct cpu_operations *cpu_ops);

#ifdef CONFIG_SMP

/* SMP initialization hook for setup_arch */
void __init setup_smp(void);

/* Hook for the generic smp_call_function_many() routine. */
void arch_send_call_function_ipi_mask(struct cpumask *mask);

/* Hook for the generic smp_call_function_single() routine. */
void arch_send_call_function_single_ipi(int cpu);

int riscv_hartid_to_cpuid(int hartid);
void cpuid_to_hartid_mask(const struct cpumask *in, struct cpumask *out);
/*
 * This is particularly ugly: it appears we can't actually get the definition
 * of task_struct here, but we need access to the CPU this task is running on.
 * Instead of using C we're using asm-offsets.h to get the current processor
 * ID.
 */
#define raw_smp_processor_id() (*((int*)((char*)get_current() + TASK_TI_CPU)))

#ifdef CONFIG_HOTPLUG_CPU
int __cpu_disable(void);
void __cpu_die(unsigned int cpu);
void cpu_play_dead(void);
void boot_sec_cpu(void);
int can_hotplug_cpu(void);
#else

static inline int can_hotplug_cpu(void) { return 0; }

#endif /* CONFIG_HOTPLUG_CPU */

#else

static inline int riscv_hartid_to_cpuid(int hartid) { return 0 ; }
static inline void cpuid_to_hartid_mask(const struct cpumask *in,
				       struct cpumask *out) {
	cpumask_set_cpu(cpu_logical_map(0), out);
}

#endif /* CONFIG_SMP */
#endif /* _ASM_RISCV_SMP_H */
