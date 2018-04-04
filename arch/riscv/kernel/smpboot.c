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
#include <linux/sched/hotplug.h>
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

#ifdef CONFIG_HOTPLUG_CPU
		arch_send_call_function_single_ipi(cpu);
#endif
		while (!cpu_online(cpu))
			cpu_relax();
		pr_notice("CPU%u: online\n", cpu);
	} else {
		pr_err("CPU %d [hartid %d]failed to boot\n", cpu, hartid);
	}

	return 0;
}

void __init smp_cpus_done(unsigned int max_cpus)
{
}

#ifdef CONFIG_HOTPLUG_CPU
int can_hotplug_cpu(void)
{
	if (cpu_ops.cpu_die)
		return 1;
	else
		return 0;
}

/*
 * __cpu_disable runs on the processor to be shutdown.
 */
int __cpu_disable(void)
{
	int ret = 0;
	unsigned int cpu = smp_processor_id();

	if (cpu_ops.cpu_disable)
		ret = cpu_ops.cpu_disable(cpu);
	if (ret)
		return ret;

	set_cpu_online(cpu, false);
	irq_migrate_all_off_this_cpu();

	return ret;
}
/*
 * called on the thread which is asking for a CPU to be shutdown -
 * waits until shutdown has completed, or it is timed out.
 */
void __cpu_die(unsigned int cpu)
{
	if (!cpu_wait_death(cpu, 5)) {
		pr_err("CPU %u: didn't die\n", cpu);
		return;
	}
	pr_notice("CPU%u: shutdown\n", cpu);
	/*TODO: Do we need to verify is cpu is really dead */
}

int default_cpu_disable(unsigned int cpu)
{
	if (!cpu_ops.cpu_die)
		return -EOPNOTSUPP;
	return 0;
}

/*
 * Called from the idle thread for the CPU which has been shutdown.
 *
 */
void cpu_play_dead(void)
{
	int cpu = smp_processor_id();

	idle_task_exit();

	(void)cpu_report_death();

	/* Do not disable software interrupt to restart cpu after WFI */
	csr_clear(sie, SIE_STIE | SIE_SEIE);
	if (cpu_ops.cpu_die)
		cpu_ops.cpu_die(cpu);
}

void default_cpu_die(unsigned int cpu)
{
	int sipval, sieval, scauseval;

	/* clear all pending flags */
	csr_write(sip, 0);
	/* clear any previous scause data */
	csr_write(scause, 0);

	do {
		wait_for_interrupt();
		sipval = csr_read(sip);
		sieval = csr_read(sie);
		scauseval = csr_read(scause);
	/* only break if wfi returns for an enabled interrupt */
	} while ((sipval & sieval) == 0 &&
		 scauseval != INTERRUPT_CAUSE_SOFTWARE);

	boot_sec_cpu();
}

#endif /* CONFIG_HOTPLUG_CPU */
/*
 * C entry point for a secondary processor.
 */
asmlinkage void smp_callin(void)
{
	struct mm_struct *mm = &init_mm;

	/* All kernel threads share the same mm context.  */
	atomic_inc(&mm->mm_count);
	current->active_mm = mm;

	trap_init();
	notify_cpu_starting(smp_processor_id());
	set_cpu_online(smp_processor_id(), true);
	local_flush_tlb_all();
	local_irq_enable();
	preempt_disable();
	cpu_startup_entry(CPUHP_AP_ONLINE_IDLE);
}

struct cpu_operations default_ops = {
	.name		= "default",
	.cpu_boot	= default_cpu_boot,
#ifdef CONFIG_HOTPLUG_CPU
	.cpu_disable	= default_cpu_disable,
	.cpu_die	= default_cpu_die,
#endif
};
