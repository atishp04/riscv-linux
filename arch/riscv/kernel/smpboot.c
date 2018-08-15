/*
 * SMP initialisation and IPI support
 * Based on arch/arm64/kernel/smp.c
 *
 * Copyright (C) 2012 ARM Ltd.
 * Copyright (C) 2015 Regents of the University of California
 * Copyright (C) 2017 SiFive
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/kernel_stat.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/percpu.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/sched/task_stack.h>
#include <linux/smp.h>
#include <asm/irq.h>
#include <asm/mmu_context.h>
#include <asm/tlbflush.h>
#include <asm/sections.h>
#include <asm/sbi.h>

void *__cpu_up_stack_pointer[NR_CPUS];
void *__cpu_up_task_pointer[NR_CPUS];
struct cpu_operations default_ops;

void __init smp_prepare_boot_cpu(void)
{
}

void __init smp_prepare_cpus(unsigned int max_cpus)
{
}

void __init setup_smp(void)
{
	struct device_node *dn = NULL;
	int hart, found_boot_cpu = 0;
	int cpuid = 1;

	smp_set_cpu_ops(&default_ops);
	while ((dn = of_find_node_by_type(dn, "cpu"))) {
		hart = riscv_of_processor_hart(dn);

		if (hart < 0)
			continue;
		if (hart == cpu_logical_map(0)) {
			BUG_ON(found_boot_cpu);
			found_boot_cpu = 1;
			continue;
		}

		cpu_logical_map(cpuid) = hart;
		set_cpu_possible(cpuid, true);
		set_cpu_present(cpuid, true);
		cpuid++;
	}

	BUG_ON(!found_boot_cpu);
}

int default_cpu_boot(unsigned int hartid, struct task_struct *tidle)
{
	/*
	 * On RISC-V systems, all harts boot on their own accord.  Our _start
	 * selects the first hart to boot the kernel and causes the remainder
	 * of the harts to spin in a loop waiting for their stack pointer to be
	 * setup by that main hart.  Writing __cpu_up_stack_pointer signals to
	 * the spinning harts that they can continue the boot process.
	 */

	__cpu_up_stack_pointer[hartid] = task_stack_page(tidle) + THREAD_SIZE;
	__cpu_up_task_pointer[hartid] = tidle;
	return 0;
}

int __cpu_up(unsigned int cpu, struct task_struct *tidle)
{
	int err = -1;
	int hartid = cpu_logical_map(cpu);

	tidle->thread_info.cpu = cpu;
	smp_mb();

	if (cpu_ops.cpu_boot)
		err = cpu_ops.cpu_boot(hartid, tidle);
	if (!err) {
		while (!cpu_online(cpu))
			cpu_relax();
	} else {
		pr_err("CPU %d [hartid %d]failed to boot\n", cpu, hartid);
	}
	return 0;
}

void __init smp_cpus_done(unsigned int max_cpus)
{
}

/*
 * C entry point for a secondary processor.
 */
asmlinkage void __init smp_callin(void)
{
	struct mm_struct *mm = &init_mm;

	/* All kernel threads share the same mm context.  */
	atomic_inc(&mm->mm_count);
	current->active_mm = mm;

	trap_init();
	notify_cpu_starting(smp_processor_id());
	set_cpu_online(smp_processor_id(), 1);
	local_flush_tlb_all();
	local_irq_enable();
	preempt_disable();
	cpu_startup_entry(CPUHP_AP_ONLINE_IDLE);
}


struct cpu_operations default_ops = {
	.name		= "default",
	.cpu_boot	= default_cpu_boot,
};
