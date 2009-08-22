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

#define FILE_FAN_MANUAL "/sys/devices/platform/applesmc.768/fan%d_manual"
#define FILE_FAN_SPEED  "/sys/devices/platform/applesmc.768/fan%d_output"

/* Starts at 0 */
#define FILE_CPU_TEMP "/sys/devices/platform/coretemp.%d/temp1_input"
