/****************************************************************************
 * shced/sched_removereadytorun.c
 *
 *   Copyright (C) 2007-2009, 2012, 2016 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <stdbool.h>
#include <queue.h>
#include <assert.h>

#include <nuttx/sched_note.h>

#include "irq/irq.h"
#include "sched/sched.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sched_removereadytorun
 *
 * Description:
 *   This function removes a TCB from the ready to run list.
 *
 * Inputs:
 *   rtcb - Points to the TCB that is ready-to-run
 *
 * Return Value:
 *   true if the currently active task (the head of the ready-to-run list)
 *     has changed.
 *
 * Assumptions:
 * - The caller has established a critical section before calling this
 *   function (calling sched_lock() first is NOT a good idea -- use
 *   enter_critical_section()).
 * - The caller handles the condition that occurs if the head of the
 *   ready-to-run list is changed.
 *
 ****************************************************************************/

#ifndef CONFIG_SMP
bool sched_removereadytorun(FAR struct tcb_s *rtcb)
{
  bool doswitch = false;

  /* Check if the TCB to be removed is at the head of the ready to run list.
   * There is only one list, g_readytorun, and it always contains the
   * currently running task.  If we are removing the head of this list,
   * then we are removing the currently active task.
   */

  if (rtcb->blink == NULL)
    {
      /* There must always be at least one task in the list (the IDLE task)
       * after the TCB being removed.
       */

      FAR struct tcb_s *ntcb = (FAR struct tcb_s *)rtcb->flink;
      DEBUGASSERT(ntcb != NULL);

      /* Inform the instrumentation layer that we are switching tasks */

      sched_note_switch(rtcb, ntcb);
      ntcb->task_state = TSTATE_TASK_RUNNING;
      doswitch = true;
    }

  /* Remove the TCB from the ready-to-run list.  In the non-SMP case, this
   * is always the g_readytorun list.
   */

  dq_rem((FAR dq_entry_t *)rtcb, (FAR dq_queue_t *)&g_readytorun);

  /* Since the TCB is not in any list, it is now invalid */

  rtcb->task_state = TSTATE_TASK_INVALID;
  return doswitch;
}
#endif /* !CONFIG_SMP */

/****************************************************************************
 * Name: sched_removereadytorun
 *
 * Description:
 *   This function removes a TCB from the ready to run list.
 *
 * Inputs:
 *   rtcb - Points to the TCB that is ready-to-run
 *
 * Return Value:
 *   true if the currently active task (the head of the ready-to-run list)
 *     has changed.
 *
 * Assumptions:
 * - The caller has established a critical section before calling this
 *   function (calling sched_lock() first is NOT a good idea -- use
 *   enter_critical_section()).
 * - The caller handles the condition that occurs if the head of the
 *   ready-to-run list is changed.
 *
 ****************************************************************************/

#ifdef CONFIG_SMP
bool sched_removereadytorun(FAR struct tcb_s *rtcb)
{
  FAR dq_queue_t *tasklist;
  bool doswitch = false;
  int cpu;

  /* Which CPU (if any) is the task running on?  Which task list holds the
   * TCB?
   */

  cpu      = rtcb->cpu;
  tasklist = TLIST_HEAD(rtcb->task_state, cpu);

  /* Check if the TCB to be removed is at the head of a ready to run list.
   * For the case of SMP, there are two lists involved:  (1) the
   * g_readytorun list that holds non-running tasks that have not been
   * assigned to a CPU, and (2) and the g_assignedtasks[] lists which hold
   * tasks assigned a CPU, including the task that is currently running on
   * that CPU.  Only this latter list contains the currently active task
   * only only removing the head of that list can result in a context
   * switch.
   *
   * The tasklist RUNNABLE attribute will inform us if the list holds the
   * currently executing and task and, hence, if a context switch could
   * occur.
   */

  if (rtcb->blink == NULL && TLIST_ISRUNNABLE(rtcb->task_state))
    {
      FAR struct tcb_s *ntcb;
      int me;

      /* There must always be at least one task in the list (the IDLE task)
       * after the TCB being removed.
       */

      ntcb = (FAR struct tcb_s *)rtcb->flink;
      DEBUGASSERT(ntcb != NULL);

      /* If we are modifying the head of some assigned task list other than
       * our own, we will need to stop that CPU.
       */

      me = this_cpu();
      if (cpu != me)
        {
          DEBUGVERIFY(up_cpu_pause(cpu));
        }

      /* Will pre-emption be disabled after the switch?  If the lockcount is
       * greater than zero, then this task/this CPU holds the scheduler lock.
       */

      if (ntcb->lockcount > 0)
        {
          /* Yes... make sure that scheduling logic knows about this */

          spin_setbit(&g_cpu_lockset, cpu, &g_cpu_locksetlock,
                      &g_cpu_schedlock);
        }
      else
        {
          /* No.. we may need to perform release our hold on the lock. */

          spin_clrbit(&g_cpu_lockset, cpu, &g_cpu_locksetlock,
                      &g_cpu_schedlock);
        }

      /* Interrupts may be disabled after the switch.  If irqcount is greater
       * than zero, then this task/this CPU holds the IRQ lock
       */

      if (ntcb->irqcount > 0)
        {
          /* Yes... make sure that scheduling logic knows about this */

          spin_setbit(&g_cpu_irqset, cpu, &g_cpu_irqsetlock,
                      &g_cpu_irqlock);
        }
      else
        {
          /* No.. we may need to perform release our hold on the lock. */

          spin_setbit(&g_cpu_irqset, cpu, &g_cpu_irqsetlock,
                      &g_cpu_irqlock);
        }

      /* Inform the instrumentation layer that we are switching tasks */

      sched_note_switch(rtcb, ntcb);
      ntcb->task_state = TSTATE_TASK_RUNNING;

      /* The task is running but the CPU that it was running on has been
       * paused.  We can now safely remove its TCB from the ready-to-run
       * task list.  In the SMP case this may be either the g_readytorun()
       * or the g_assignedtasks[cpu] list.
       */

      dq_rem((FAR dq_entry_t *)rtcb, tasklist);

      /* All done, restart the other CPU (if it was paused). */

      doswitch = true;
      if (cpu != me)
        {
          /* In this we will not want to report a context switch to this
           * CPU.  Only the other CPU is affected.
           */

          DEBUGVERIFY(up_cpu_resume(cpu));
          doswitch = false;
        }
    }
  else
    {
      /* The task is not running.  Just remove its TCB from the ready-to-run
       * list.  In the SMP case this may be either the g_readytorun() or the
       * g_assignedtasks[cpu] list.
       */

      dq_rem((FAR dq_entry_t *)rtcb, tasklist);
    }

  /* Since the TCB is no longer in any list, it is now invalid */

  rtcb->task_state = TSTATE_TASK_INVALID;
  return doswitch;
}
#endif /* CONFIG_SMP */
