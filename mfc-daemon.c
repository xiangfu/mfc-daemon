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

#define QUIT_DAEMON(message, ...) quit_daemon(message " [%s:%d]", ##__VA_ARGS__, __FUNCTION__, __LINE__)

void write_fan_2_manual(int);
void write_fan_2_speed(int);

int read_cpu_1_temp(void);
int read_cpu_2_temp(void);

void write_fan_1_manual(int);
void write_fan_1_speed(int);

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


void Signal_Handler(int sig){
	switch(sig){
		case SIGHUP:
			break;
		case SIGTERM:
			syslog(LOG_INFO, "Signal_Handler");
			write_fan_1_manual(0);
			if (MFC.cpucount > 1) {
				write_fan_2_manual(0);
			}
			QUIT_DAEMON("Stop");
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
	write_fan_1_manual(1);
	if (MFC.cpucount > 1) {
		write_fan_2_manual(1);
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

	int rd_cpu_1_temp=read_cpu_1_temp();
	int rd_cpu_2_temp=read_cpu_2_temp();

	int temp=(rd_cpu_1_temp + rd_cpu_2_temp)/2000;
	int old_temp=(rd_cpu_1_temp + rd_cpu_2_temp)/2000;
	int fan_speed=GET_FAN_SPEED(temp);

	fan_speed=set_min_max_fan_speed(fan_speed);
	write_fan_1_speed(fan_speed);	
	if (MFC.cpucount > 1) {
		write_fan_2_speed(fan_speed);
	}

	while(1){

		rd_cpu_1_temp = read_cpu_1_temp();
		rd_cpu_2_temp = read_cpu_2_temp();

		wr_manual++;

		if (wr_manual==9){
			write_fan_1_manual(1);
			if (MFC.cpucount > 1) {
				write_fan_2_manual(1);
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
				write_fan_1_speed(fan_speed);
				if (MFC.cpucount > 1) {
					write_fan_2_speed(fan_speed);
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

int read_cpu_1_temp(void){
	int temp;
	FILE *file;

	if ((file=fopen(RD_CPU_1_TEMP,"r"))!=NULL){
		fscanf(file,"%d",&temp);
		fclose(file);
		return temp;
	}
	QUIT_DAEMON("Failed to read the temperature of CPU 1");
	return 0;
}

int read_cpu_2_temp(void){
	int temp;
	FILE *file;

	if (MFC.cpucount == 1) {
		// If there's a single core pretend that the second core has the same
		// temperature as the first core.
		return read_cpu_1_temp();
	}
	
	if ((file=fopen(RD_CPU_2_TEMP,"r"))!=NULL){
		fscanf(file,"%d", &temp);
		fclose(file);
		return temp;
	}
	QUIT_DAEMON("Failed to read the temperature of CPU 2");
	return 0;
}

void write_fan_1_speed(int fan_speed_1){
	FILE *file;

	if((file=fopen(WR_FAN_1, "w"))!=NULL){
		fprintf(file,"%d",fan_speed_1);
		fclose(file);
		return;
	}
	QUIT_DAEMON("Failed to set the speed of fan 1, is applesmc loaded?");
}

void write_fan_1_manual(int fan_manual_1){
	FILE *file;
	if((file=fopen(FAN_1_MANUAL, "w"))!=NULL){
		fprintf(file,"%d",fan_manual_1);
		fclose(file);
	}else{
		QUIT_DAEMON("Failed to set the 'manual' of fan 1, is applesmc loaded?");
	}
}

void write_fan_2_speed(int fan_speed_2){
	FILE *file;

	if((file=fopen(WR_FAN_2, "w"))!=NULL){
		fprintf(file,"%d",fan_speed_2);
		fclose(file);
	}
	else{
		QUIT_DAEMON("Failed to set the speed of fan 1, is applesmc loaded?");
	}
}

void write_fan_2_manual(int fan_manual_2){
	FILE *file;
	if((file=fopen(FAN_2_MANUAL, "w"))!=NULL){
		fprintf(file,"%d",fan_manual_2);
		fclose(file);
	}
	else{
		QUIT_DAEMON("Failed to set the 'manual' of fan 1, is applesmc loaded?");
	}
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
	FILE *file;
	if((file=fopen(PIDFILE,"w"))!=NULL){
		fprintf(file,"%d",getpid());
		fclose(file);
	}
	else{
		QUIT_DAEMON("Can't write PID file %s", PIDFILE);
	}
}


void check_pidfile(){
	FILE *file;
	if((file=fopen(PIDFILE,"r"))!=NULL){
		/* if PIDFILE exist */
		fclose(file);
		QUIT_DAEMON("PID file %s already exists, is the daemon running?", PIDFILE);
	}
}


int check_cpu(){
	FILE *file;
	char buffer[80];
	int cpucount=0;

	if((file=fopen(CPUINFO,"r"))!=NULL){
		while (!feof(file)) {
			fgets(buffer, sizeof(buffer),file);
			if (!strncmp(buffer,"model name	: Intel(R) Core(TM)2 Duo CPU",39)){
				cpucount++;
				if (cpucount==1){
					syslog(LOG_INFO,"CPU: %s",buffer);
				}
			}
		}
		fclose(file);
		syslog(LOG_INFO,"cpu counts %d", cpucount);
	}	
	else{
		QUIT_DAEMON("Can't read the CPU type from %s", CPUINFO);
	}

	return cpucount;
}
