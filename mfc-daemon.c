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
#include "config.h"

#define ERROR -1
#define OK 0

#define MODE_WRITE "w"
#define MODE_READ  "r"

#define CPU_LABEL  "model name	: Intel(R) Core(TM)2 Duo CPU"

#define QUIT_DAEMON(message, ...) quit_daemon(message " [%s:%d]", ##__VA_ARGS__, __FUNCTION__, __LINE__)

void write_fan_manual(int, int);
void write_fan_speed(int, int);
int read_cpu_temp(int);

void write_pidfile(void);
void check_pidfile(void);
int check_cpu(void);
int log_fan_speed(int,int,int);
int set_min_max_fan_speed(int);


/**
 * The daemon's global context. This data structure contains all global
 * variables needed by daemon.
 */
typedef struct _MfcCtx {
	int cpucount;
	int pidfile_created;
} MfcCtx;

MfcCtx MFC = {0,};

void quit_daemon (char *format, ...) {
	va_list args;

	va_start(args, format);
	vsyslog(LOG_ERR, format, args);
//	vprintf(format, args);
//	printf("\n");
	va_end(args);

	closelog();
	if (MFC.pidfile_created) {
		unlink(PIDFILE);
	}
	exit(ERROR);
}

FILE* mfc_fopen (const char* mode, const char *format, ...) {
	char *filename;
	int code;
	va_list args;
	FILE *file;

	va_start(args, format);
	code = vasprintf(&filename, format, args);
	va_end(args);

	if (code < 0) {
		QUIT_DAEMON("Failed to allocate filename for pattern %s", format);
	}

	file = fopen(filename, mode);
	free(filename);

	return file;
}

void Signal_Handler(int sig){
	switch(sig){
		case SIGHUP:
		break;

		case SIGTERM:
			{
				int cpu;
				syslog(LOG_INFO, "Signal_Handler");

				for (cpu = 1; cpu <= MFC.cpucount; ++cpu) {
					write_fan_manual(cpu, 0);
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
		exit(OK);
		/* child continues */
	}

	if(setsid() == ERROR){
		QUIT_DAEMON("Error setsid");
	}

	for (i=getdtablesize();i>=0;--i) close(i);
	umask(027);
	chdir("/");
}


int main(int argc, char **argv){

	signal(SIGHUP,Signal_Handler);		/* hangup signal */
	signal(SIGTERM,Signal_Handler);		/* software termination signal from kill */

	struct timespec timx,tim1;

	openlog("mfc-daemon", LOG_PID, LOG_DAEMON);


	/* check machine and pidfile*/
	MFC.cpucount = check_cpu();
	check_pidfile();
	write_pidfile();
	MFC.pidfile_created = 1;
	write_fan_manual(1, 1);
	if (MFC.cpucount > 1) {
		write_fan_manual(1, 1);
	}
	start_daemon();

	tim1.tv_sec = TV_SEC;
	tim1.tv_nsec = TV_NSEC;

	//init
	int cold=2;
	int hot=2;
	int wr_manual=0;
	int change_number=0;
	int old_fan_speed=-1;

	syslog(LOG_INFO,"Start");

	int rd_cpu_1_temp=read_cpu_temp(1);
	int rd_cpu_2_temp=read_cpu_temp(2);

	int temp=(rd_cpu_1_temp + rd_cpu_2_temp)/2000;
	int old_temp=(rd_cpu_1_temp + rd_cpu_2_temp)/2000;
	int fan_speed=GET_FAN_SPEED(temp);

	fan_speed=set_min_max_fan_speed(fan_speed);

	int cpu;
	for (cpu = 1; cpu <= MFC.cpucount; ++cpu) {
		write_fan_speed(cpu, fan_speed);
	}
	while(1){

		rd_cpu_1_temp = read_cpu_temp(1);
		rd_cpu_2_temp = read_cpu_temp(2);

		wr_manual++;

		if (wr_manual==9){
			for (cpu = 1; cpu <= MFC.cpucount; ++cpu) {
				write_fan_manual(cpu, 1);
			}
			wr_manual=0;
		}

		temp=(rd_cpu_1_temp+rd_cpu_2_temp)/2000;

		if (temp<old_temp){
			cold++;
			hot=0;
		}
		if (temp>old_temp){
			hot++;
			cold=0;
		}

		if ((cold==3)||(hot==3)){
			//	temp = average of both cpu's
			fan_speed=GET_FAN_SPEED(temp);
			fan_speed=set_min_max_fan_speed(fan_speed);

			if (fan_speed!=old_fan_speed){
				for (cpu = 1; cpu <= MFC.cpucount; ++cpu) {
					write_fan_speed(cpu, fan_speed);
				}
				change_number=log_fan_speed(fan_speed,change_number,temp);
				old_fan_speed=fan_speed;
			}

			cold=0;
			hot=0;
		}

		old_temp=temp;

		if (nanosleep(&tim1,&timx) < OK){
			QUIT_DAEMON("Error nanosleep");
		}
	}
}

int read_cpu_temp(int index){
	int temp;
	FILE *file;

	if (MFC.cpucount == 1) {
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
	syslog(LOG_INFO, "Change %d: fan speed %d RPM temperature %d degree celsius", change_number, fan_speed, temp);
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
	FILE *file = mfc_fopen(MODE_READ, PIDFILE);
	if (file == NULL) {
		/* We are expecting that the fiel DOES NOT exist, so there should be an error */
		return;
	}

	/* if PIDFILE exist */
	fclose(file);
	QUIT_DAEMON("PID file %s already exists, is the daemon running?", PIDFILE);
}


int check_cpu(){
	FILE *file;
	char *line = NULL;
	size_t size = 0;
	int cpucount = 0;

	file = mfc_fopen(MODE_READ, CPUINFO);
	if (file == NULL) {
		QUIT_DAEMON("Can't read the CPU type from %s", CPUINFO);
	}

	while (!feof(file)) {
		getline(&line, &size, file);
		if (!strncmp(line, CPU_LABEL, strlen(CPU_LABEL))) {
			cpucount++;
			if (cpucount == 1) {
				syslog(LOG_INFO, "CPU: %s", line);
			}
		}
	}

	fclose(file);
	syslog(LOG_INFO, "cpu counts %d", cpucount);

	return cpucount;
}
