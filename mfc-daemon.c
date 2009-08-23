/*
 * mfc-daemon	macbook auto fan control deamon
 *
 * (C) Copyright 2009
 * Author: Xiangfu Liu <xiangfu.z@gmail.com>
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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <syslog.h>
#include <stdarg.h>
#include <errno.h>
#include <getopt.h>
#include "config.h"

#define TRUE  1
#define FALSE 0

#define MODE_WRITE "w"
#define MODE_READ  "r"

#define CPU_LABEL  "model name	: Intel(R) Core(TM)2 Duo CPU"

#define QUIT_DAEMON(message, ...) quit_daemon(message " [%s:%d]", ##__VA_ARGS__, __FUNCTION__, __LINE__)
#define FILE_EXISTS(filename) (access(filename, F_OK) == 0)

#define INFO(message, ...)  LOG(LOG_INFO, message, ##__VA_ARGS__)
#define ERROR(message, ...) LOG(LOG_ERR,  message, ##__VA_ARGS__)
#define LOG(level, message, ...) mfc_log(level,  message " [%s:%d]", ##__VA_ARGS__, __FUNCTION__, __LINE__)

#define OPTION_END { 0, }
#define OPTION_NOARG(name, letter, var)	{ name, no_argument, var, letter }
#define OPTION_REQUIRED(name, letter, var) { name, required_argument, var, letter }
#define OPTION_OPTIONAL(name, letter, var) { name, optional_argument, var, letter }


/**
 * The daemon's global context. This data structure contains all global
 * variables needed by daemon.
 */
typedef struct _MfcCtx {
	int total_cpus;
	int total_fans;
	int pidfile;
	int syslog;
	int fork;
	int stdout;
} MfcCtx;

MfcCtx MFC = {0,};


void write_fan_manual(int, int);
void write_fan_speed(int, int);
int read_cpu_temp(int);

void write_pidfile(void);
void check_pidfile(void);
int check_cpu(void);
int check_fan(void);
int log_fan_speed(int,int,int);
int set_min_max_fan_speed(int);
int get_cpu_temperature(void);
void parse_options (int argc, char * const argv[]);


void mfc_log (int level, char *format, ...) {
	va_list args;

	va_start(args, format);

	if (MFC.syslog) {
		vsyslog(level, format, args);
	}

	if (MFC.stdout) {
		vprintf(format, args);
		printf("\n");
	}

	va_end(args);
}

void quit_daemon (char *format, ...) {
	va_list args;

	va_start(args, format);


	if (MFC.syslog) {
		vsyslog(LOG_ERR, format, args);
		closelog();
	}

	if (MFC.stdout) {
		vprintf(format, args);
		printf("\n");
	}

	va_end(args);

	if (MFC.pidfile) {
		unlink(PIDFILE);
	}
	exit(EXIT_FAILURE);
}

char* mfc_vsprintf (const char *format, va_list args) {
	char *string;
	int code;

	code = vasprintf(&string, format, args);
	if (code < 0) {
		va_end(args);
		QUIT_DAEMON("Failed to allocate filename for pattern %s", format);
	}

	return string;
}

char* mfc_sprintf (const char *format, ...) {
	char *string;
	va_list args;

	va_start(args, format);
	string = mfc_vsprintf(format, args);
	va_end(args);

	return string;
}

FILE* mfc_fopen (const char* mode, const char *format, ...) {
	char *filename;
	va_list args;
	FILE *file;

	va_start(args, format);
	filename = mfc_vsprintf(format, args);
	va_end(args);

	errno = 0;
	file = fopen(filename, mode);
	if (file == NULL) {
		ERROR(
			"Failed to open %s in mode %s because: %s",
			filename, mode, strerror(errno)
		);
	}
	free(filename);

	return file;
}

void Signal_Handler(int sig){
	switch(sig){
		case SIGHUP:
		break;

		case SIGTERM:
			{
				int fan;
				INFO("Signal handler caught SIGTERM");

				for (fan = 1; fan <= MFC.total_fans; ++fan) {
					write_fan_manual(fan, 0);
				}
				QUIT_DAEMON("Stop");
			}
		break;
	}
}

void start_daemon(void){
	int i=0;
	pid_t pid;
	pid=fork();

	if (pid<0){
		/* fork error */
		QUIT_DAEMON("Error cannot fork");
	}
	else if (pid>0){
		exit(EXIT_SUCCESS);
		/* child continues */
	}

	if(setsid() == -1){
		QUIT_DAEMON("Error setsid");
	}

	for (i=getdtablesize();i>=0;--i) close(i);
	umask(027);
	chdir("/");
}


int main(int argc, char * const argv[]){

	/* Parse the command line arguments */
	MFC.fork = TRUE;
	parse_options(argc, argv);

	signal(SIGHUP,Signal_Handler);		/* hangup signal */
	signal(SIGTERM,Signal_Handler);		/* software termination signal from kill */

	struct timespec timx,tim1;

	openlog("mfc-daemon", LOG_PID, LOG_DAEMON);
	MFC.syslog = TRUE;


	/* check machine and pidfile*/
	MFC.total_cpus = check_cpu();
	MFC.total_fans = check_fan();
	check_pidfile();
	write_pidfile();
	MFC.pidfile = TRUE;

	if (MFC.fork) {
		start_daemon();
	}

	int fan;
	for (fan = 1; fan <= MFC.total_fans; ++fan) {
		write_fan_manual(fan, 1);
	}

	tim1.tv_sec = TV_SEC;
	tim1.tv_nsec = TV_NSEC;

	//init
	int cold=2;
	int hot=2;
	int wr_manual=0;
	int change_number=0;
	int old_fan_speed=-1;

	INFO("Start");

	int temp = get_cpu_temperature();
	int old_temp = temp;
	int fan_speed=GET_FAN_SPEED(temp);

	fan_speed=set_min_max_fan_speed(fan_speed);

	for (fan = 1; fan <= MFC.total_fans; ++fan) {
		write_fan_speed(fan, fan_speed);
	}

	while (1){

		wr_manual++;
		if (wr_manual==9){
			for (fan = 1; fan <= MFC.total_fans; ++fan) {
				write_fan_manual(fan, 1);
			}
			wr_manual=0;
		}

		temp = get_cpu_temperature();
		if (temp < old_temp){
			cold++;
			hot = 0;
		}
		else if (temp > old_temp){
			hot++;
			cold = 0;
		}

		if ((cold==3)||(hot==3)){
			//	temp = average of both cpu's
			fan_speed=GET_FAN_SPEED(temp);
			fan_speed=set_min_max_fan_speed(fan_speed);

			if (fan_speed!=old_fan_speed){
				for (fan = 1; fan <= MFC.total_fans; ++fan) {
					write_fan_speed(fan, fan_speed);
				}
				change_number=log_fan_speed(fan_speed,change_number,temp);
				old_fan_speed=fan_speed;
			}

			cold=0;
			hot=0;
		}

		old_temp=temp;

		if (nanosleep(&tim1,&timx) == -1){
			QUIT_DAEMON("Error nanosleep");
		}
	}
}

int read_cpu_temp(int index){
	int temp;
	FILE *file;

	if (MFC.total_cpus == 1) {
		// If there's a single core pretend that the second core has the same
		// temperature as the first core.
		index = 1;
	}

	/* The CPU temperature file index starts at 0 */
	file = mfc_fopen(MODE_READ, FILE_CPU_TEMP, index - 1);
	if (file == NULL) {
		QUIT_DAEMON("Failed to read the temperature of CPU %d", index);
	}

	fscanf(file, "%d", &temp);
	fclose(file);
	return temp;
}

void write_fan_speed(int index, int speed){
	FILE *file = mfc_fopen(MODE_WRITE, FILE_FAN_SPEED, index);
	if (file == NULL) {
		QUIT_DAEMON("Failed to set the speed of fan %d, is applesmc loaded?", index);
	}

	fprintf(file, "%d", speed);
	fclose(file);
}

void write_fan_manual(int index, int manual){
	FILE *file = mfc_fopen(MODE_WRITE, FILE_FAN_MANUAL, index);
	if (file == NULL) {
		QUIT_DAEMON("Failed to change the manual setting of fan %d, is applesmc loaded?", index);
	}

	fprintf(file, "%d", manual);
	fclose(file);
}

int set_min_max_fan_speed(int fan_speed){

	if (fan_speed<MIN_SPEED){
		fan_speed=MIN_SPEED;
	}
	if (fan_speed>MAX_SPEED){
		fan_speed=MAX_SPEED;
	}

	return fan_speed;
}

int log_fan_speed(int fan_speed,int change_number,int temp){
	change_number++;
	INFO("Change %d: fan speed %d RPM temperature %d degree celsius", change_number, fan_speed, temp);
	return change_number;
}

void write_pidfile(){
	FILE *file = mfc_fopen(MODE_WRITE, PIDFILE);
	if (file == NULL) {
		QUIT_DAEMON("Can't write PID file %s", PIDFILE);
	}

	fprintf(file,"%d",getpid());
	fclose(file);
}


void check_pidfile(){
	if (FILE_EXISTS(PIDFILE)) {
		/* We are expecting that the file DOES NOT exist */
		QUIT_DAEMON("PID file %s already exists, is the daemon running?", PIDFILE);
	}

	return;
}


int check_cpu(){
	FILE *file;
	char *line = NULL;
	size_t size = 0;
	int total_cpus = 0;

	file = mfc_fopen(MODE_READ, CPUINFO);
	if (file == NULL) {
		QUIT_DAEMON("Can't read the CPU type from %s", CPUINFO);
	}

	while (!feof(file)) {
		getline(&line, &size, file);
		if (!strncmp(line, CPU_LABEL, strlen(CPU_LABEL))) {
			total_cpus++;
			if (total_cpus == 1) {
				syslog(LOG_INFO, "CPU: %s", line);
			}
		}
	}

	fclose(file);
	INFO("Detected %d CPUs", total_cpus);

	return total_cpus;
}


int check_fan() {

	int fan = 0;
	for (fan = 1; fan <= 64; ++fan) {
		char *filename = mfc_sprintf(FILE_FAN_MANUAL, fan);
		int exists = FILE_EXISTS(filename);
		free(filename);

		if (!exists) {
			--fan;
			break;
		}
	}

	INFO("Detected %d fans", fan);

	return fan;
}


int get_cpu_temperature() {

	int total_cpus = MFC.total_cpus;
	if (total_cpus == 1) {
		/* Assume that the computer has 2 CPUs (2 cores). Some Mac laptops have
		   to be booted with maxcores=1 or acpi=off, this is true for the
		   MacBook 5,1. Both options diseable the second core and Linux sees a
		   single one.

		   When computing the temperature it could be wiser to assume that the
		   temperature was computed for two cores. Although I'm not too sure
		   about this.
		 */
		total_cpus = 2;
	}

	int temp = 0;
	int cpu;
	for (cpu = 1; cpu <= total_cpus; ++cpu) {
		temp += read_cpu_temp(cpu);
	}
	temp /= 2000;

	return temp;
}


void parse_options (int argc, char * const argv[]) {

	struct option longopts[] = {
		OPTION_NOARG("help", 'h', NULL),
		OPTION_NOARG("no-fork", FALSE, &MFC.fork),
		OPTION_NOARG("stdout", 's', &MFC.stdout),
		OPTION_END,
	};

	while (TRUE) {
		int c = getopt_long(argc, argv, "hXs", longopts, NULL);
		if (c == -1) {
			break;
		}

		switch (c) {
			case 'h':
				{
					QUIT_DAEMON("Usage: mfc-daemon [OPTION]...");
				}
			break;
		}
	}
}

