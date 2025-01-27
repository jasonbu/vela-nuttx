/****************************************************************************
 * arch/arm/src/armv6-m/arm_dumpnvic.c
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/coredump.h>
#include <sys/types.h>
#include <debug.h>

#include <nuttx/irq.h>

#include "arm_internal.h"
#include "nvic.h"

#ifdef CONFIG_DEBUG_FEATURES

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Function:  arm_dumpnvic
 *
 * Description:
 *   Dump all NVIC and SYSCON registers along with a user message.
 *
 ****************************************************************************/

void arm_dumpnvic(const char *msg)
{
#ifdef CONFIG_DEBUG_INFO
  irqstate_t flags;
  int i;

  /* The following requires exclusive access to the NVIC/SYSCON registers */

  flags = enter_critical_section();

  _info("NVIC: %s\n", msg);
  _info("   ISER: %08" PRIx32 "  ICER: %08" PRIx32
        "  ISPR: %08" PRIx32 "  ICPR: %08" PRIx32 "\n",
        getreg32(ARMV6M_NVIC_ISER), getreg32(ARMV6M_NVIC_ICER),
        getreg32(ARMV6M_NVIC_ISPR), getreg32(ARMV6M_NVIC_ICPR));

  for (i = 0 ; i < 8; i += 4)
    {
      _info("   IPR%d: %08" PRIx32 "  IPR%d: %08" PRIx32
            "  IPR%d: %08" PRIx32 "  IPR%d: %08" PRIx32 "\n",
            i,     getreg32(ARMV6M_NVIC_IPR(i)),
            i + 1, getreg32(ARMV6M_NVIC_IPR(i + 1)),
            i + 2, getreg32(ARMV6M_NVIC_IPR(i + 2)),
            i + 3, getreg32(ARMV6M_NVIC_IPR(i + 3)));
    }

  _info("SYSCON:\n");
  _info("  CPUID: %08" PRIx32 "  ICSR: %08" PRIx32
        " AIRCR: %08" PRIx32 "   SCR: %08" PRIx32 "\n",
        getreg32(ARMV6M_SYSCON_CPUID), getreg32(ARMV6M_SYSCON_ICSR),
        getreg32(ARMV6M_SYSCON_AIRCR), getreg32(ARMV6M_SYSCON_SCR));
  _info("    CCR: %08" PRIx32 " SHPR2: %08" PRIx32
        " SHPR3: %08" PRIx32 "\n",
        getreg32(ARMV6M_SYSCON_CCR),   getreg32(ARMV6M_SYSCON_SHPR2),
        getreg32(ARMV6M_SYSCON_SHPR3));

  leave_critical_section(flags);
#endif
}

#endif /* CONFIG_DEBUG_FEATURES */

#ifdef CONFIG_ARM_COREDUMP_REGION

/****************************************************************************
 * Function:  arm_coredump_add_region
 *
 * Description:
 *   Dump all NVIC registers during a core dump.
 *
 ****************************************************************************/

void arm_coredump_add_region(void)
{
  coredump_add_memory_region((uint32_t *)ARMV6M_NVIC1_BASE,
                             ARMV6M_NVIC_IPR7 + 4 - ARMV6M_NVIC1_BASE,
                             PF_REGISTER);
}

#endif /* CONFIG_ARM_COREDUMP_REGION */
