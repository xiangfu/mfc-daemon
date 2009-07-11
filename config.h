/*
 * mfc-daemon	macbook auto fan control deamon
 *
 * (C) Copyright 2009
 * Author: Xiangfu liu <xiangfu.z@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 3 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA
 */

#define MACBOOK51	1	/* now it's support MacBook5.1 */

#define MIN_SPEED 2000
#define MAX_SPEED 6000

#define CPUINFO "/proc/cpuinfo"
#define PIDFILE "/var/run/mfc-daemon.pid"

#define GET_FAN_SPEED(t) (((t) - 38) * 180) 
//	(50 - 38) * 160 = 1920
//	(60 - 38) * 160 = 3520
//	(70 - 38) * 160 = 5120
#define TV_SEC	5		/*  */
#define TV_NSEC	0		/*  */

#define FAN_1_MANUAL "/sys/devices/platform/applesmc.768/fan1_manual"
#define RD_FAN_1 "/sys/devices/platform/applesmc.768/fan1_input"
#define WR_FAN_1 "/sys/devices/platform/applesmc.768/fan1_output"

#define RD_CPU_1_TEMP "/sys/devices/platform/coretemp.0/temp1_input"
#define RD_CPU_2_TEMP "/sys/devices/platform/coretemp.1/temp1_input"

#ifndef MACBOOK51
#define FAN_2_MANUAL "/sys/devices/platform/applesmc.768/fan2_manual"
#define RD_FAN_2 "/sys/devices/platform/applesmc.768/fan2_input"
#define WR_FAN_2 "/sys/devices/platform/applesmc.768/fan2_output"
#endif
