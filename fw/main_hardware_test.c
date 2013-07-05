/*
 ChibiOS/RT - Copyright (C) 2006,2007,2008,2009,2010,
 2011 Giovanni Di Sirio.

 This file is part of ChibiOS/RT.

 ChibiOS/RT is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 3 of the License, or
 (at your option) any later version.

 ChibiOS/RT is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>

#include "ch.h"
#include "hal.h"
#include "halconf.h"
#include "test.h"
#include "shell.h"
#include "chprintf.h"

#define WA_SIZE_1K      THD_WA_SIZE(1024)

uint16_t pwm_cycles = 0;
uint16_t measure = 0;
uint16_t tmp1 = 0;
uint16_t tmp2 = 0;

/*===========================================================================*/
/* Command line related.                                                     */
/*===========================================================================*/

#define SHELL_WA_SIZE   THD_WA_SIZE(4096)
#define TEST_WA_SIZE    THD_WA_SIZE(1024)

static void cmd_mem(BaseSequentialStream *chp, int argc, char *argv[]) {
  size_t n, size;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: mem\r\n");
    return;
  }
  n = chHeapStatus(NULL, &size);
  chprintf(chp, "core free memory : %u bytes\r\n", chCoreStatus());
  chprintf(chp, "heap fragments   : %u\r\n", n);
  chprintf(chp, "heap free total  : %u bytes\r\n", size);
}

static void cmd_threads(BaseSequentialStream *chp, int argc, char *argv[]) {
  static const char *states[] =
    {THD_STATE_NAMES};
  Thread *tp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: threads\r\n");
    return;
  }
  chprintf(chp, "    addr    stack prio refs     state time\r\n");
  tp = chRegFirstThread();
  do {
    chprintf(chp, "%.8lx %.8lx %4lu %4lu %9s %lu\r\n", (uint32_t)tp,
             (uint32_t)tp->p_ctx.r13, (uint32_t)tp->p_prio,
             (uint32_t)(tp->p_refs - 1), states[tp->p_state],
             (uint32_t)tp->p_time);
    tp = chRegNextThread(tp);
  } while (tp != NULL);
}

static void cmd_test(BaseSequentialStream *chp, int argc, char *argv[]) {
  Thread *tp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: test\r\n");
    return;
  }
  tp = chThdCreateFromHeap(NULL, TEST_WA_SIZE, chThdGetPriority(), TestThread,
                           chp);
  if (tp == NULL) {
    chprintf(chp, "out of memory\r\n");
    return;
  }
  chThdWait(tp);
}

static void cmd_en(BaseSequentialStream *chp, int argc, char *argv[]) {
  Thread *tp;

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: en\r\n");
    return;
  }

  palClearPad(MAX3232_GPIO, MAX3232_EN);
}

static void cmd_dis(BaseSequentialStream *chp, int argc, char *argv[]) {

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: dis\r\n");
    return;
  }

  palSetPad(MAX3232_GPIO, MAX3232_EN);
}

static void cmd_pwm(BaseSequentialStream *chp, int argc, char *argv[]) {

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: p\r\n");
    return;
  }

  pwm_cycles = 8;

//  palClearPad(MAX3232_GPIO, MAX3232_EN);
//  chThdSleepMilliseconds(10);

  pwmEnableChannel(&PWM_DRIVER, 2, 25);
  pwmEnableChannel(&PWM_DRIVER, 3, 25);
}

static void cmd_measure(BaseSequentialStream *chp, int argc, char *argv[]) {

  (void)argv;
  if (argc > 0) {
    chprintf(chp, "Usage: m\r\n");
    return;
  }

  pwm_cycles = 0;

//  palClearPad(MAX3232_GPIO, MAX3232_EN);
//  chThdSleepMilliseconds(10);
  pwmEnableChannel(&PWM_DRIVER, 0, 40);
  pwmEnableChannel(&PWM_DRIVER, 2, 25);
  pwmEnableChannel(&PWM_DRIVER, 3, 25);
  gptStartOneShot(&GPT_DRIVER, 0xFFFF);
}

static const ShellCommand commands[] =
  {
    {"mem", cmd_mem},
     {"threads", cmd_threads},
     {"test", cmd_test},
     {"en", cmd_en},
     {"dis", cmd_dis},
     {"p", cmd_pwm},
     {"m", cmd_measure},
     {NULL, NULL}};

static const ShellConfig shell_cfg1 =
  {(BaseSequentialStream *)&SERIAL_DRIVER, commands};

/*===========================================================================*/
/* PWM related.                                                              */
/*===========================================================================*/

/*
 * PWM callbacks.
 */
static void pwm1cb(PWMDriver *pwmp) {
  (void)pwmp;

  chSysLockFromIsr();
  pwm_cycles++;
  if (pwm_cycles > 160) {
	  pwmDisableChannelI(pwmp, 0);
	  pwm_cycles = 0;
	  extChannelEnableI(&EXT_DRIVER, ECHO);
	  tmp2 = (&GPT_DRIVER)->tim->CNT;
  }
  chSysUnlockFromIsr();
}

static void pwm3cb(PWMDriver *pwmp) {

  chSysLockFromIsr();
  if (pwm_cycles > 8) {
	  pwmDisableChannelI(pwmp, 2);
	  pwmDisableChannelI(pwmp, 3);
	  tmp1 = (&GPT_DRIVER)->tim->CNT;
  }
  chSysUnlockFromIsr();
}

/*
 * PWM configuration structure.
 * Update event callback enabled, channels 1 and 2 enabled without callback,
 * complementary active states.
 */
static PWMConfig pwmcfg = {
  2000000,                       /* 2Mhz PWM clock frequency.             */
  50,                            /* PWM period 25us - 40Khz (in ticks).   */
  NULL,
  {
    {PWM_OUTPUT_DISABLED, pwm1cb},
    {PWM_OUTPUT_DISABLED, NULL},
    {PWM_OUTPUT_ACTIVE_HIGH, pwm3cb},
    {PWM_OUTPUT_ACTIVE_LOW, NULL},
  },
  /* HW dependent part.*/
  0
};

/*===========================================================================*/
/* GPT related.                                                              */
/*===========================================================================*/

/*
 * GPT callback.
 */
static void gptcb(GPTDriver *gptp) {

  (void)gptp;
  chSysLockFromIsr();
  extChannelDisableI(&EXT_DRIVER, ECHO);
  measure = 0xFFFF;
  chSysUnlockFromIsr();
  palSetPad(LED_GPIO, LED3);
}

/*
 * GPT configuration.
 */
static const GPTConfig gptcfg = {
  360000,    /* 360KHz timer clock.*/
  gptcb       /* Timer callback.*/
};

/*===========================================================================*/
/* EXT related.                                                              */
/*===========================================================================*/

/*
 * EXTI callback.
 * Triggered when echo signal is received.
 */
static void ext6cb(EXTDriver *extp, expchannel_t channel) {
  uint32_t tmp;
  (void)extp;
  (void)channel;

  chSysLockFromIsr();
  tmp = (&GPT_DRIVER)->tim->CNT;
  if (tmp > 1500) {
    measure = tmp;
    gptStopTimerI(&GPT_DRIVER);
  }
  chSysUnlockFromIsr();
}

/*
 * EXT configuration.
 */

static const EXTConfig extcfg = {
  {
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_RISING_EDGE | EXT_MODE_GPIOB, ext6cb},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL},
    {EXT_CH_MODE_DISABLED, NULL}
  }
};

/*===========================================================================*/
/* Application threads.                                                      */
/*===========================================================================*/

/*
 * Red LED blinker thread, times are in milliseconds.
 */
static WORKING_AREA(waThread1, 128);
static msg_t Thread1(void *arg) {

  (void)arg;

  chRegSetThreadName("blinker");
  while (TRUE) {
    palTogglePad(LED_GPIO, LED1);
    palTogglePad(LED_GPIO, LED2);
    palTogglePad(LED_GPIO, LED3);
    palTogglePad(LED_GPIO, LED4);
    chThdSleepMilliseconds(500);
  }
  return 0;
}

/*
 * Application entry point.
 */
int main(void) {
  Thread *shelltp = NULL;

  /*
   * System initializations.
   * - HAL initialization, this also initializes the configured device drivers
   *   and performs the board-specific initializations.
   * - Kernel initialization, the main() function becomes a thread and the
   *   RTOS is active.
   */
  halInit();
  chSysInit();

  /*
   * Activates the serial driver 1 using the driver default configuration.
   */
  sdStart(&SERIAL_DRIVER, NULL);

  /*
   * Initializes the PWM driver.
   */
  pwmStart(&PWM_DRIVER, &pwmcfg);

  /*
   * Initializes the GPT driver.
   */
  gptStart(&GPT_DRIVER, &gptcfg);

  /*
   * Initializes the EXT driver.
   */
  extStart(&EXT_DRIVER, &extcfg);

  /*
   * Shell manager initialization.
   */
  shellInit();

  /*
   * Creates the blinker thread.
   */
  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  /*
   * Normal main() thread activity, in this demo it does nothing except
   * sleeping in a loop and check the button state.
   */
  while (TRUE) {
    if (!shelltp)
      shelltp = shellCreate(&shell_cfg1, SHELL_WA_SIZE, NORMALPRIO);
    else if (chThdTerminated(shelltp)) {
      chThdRelease(shelltp);
      shelltp = NULL;
    }
    chprintf(&SERIAL_DRIVER, "M: %6umm T1: %6u T2: %6u\r\n", measure / 2, tmp1, tmp2);
    chThdSleepMilliseconds(200);
  }
}
