#
# (C) Copyright  2009 
# Author: xiangjun liu <liuxiangj@gmail.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# version 3 as published by the Free Software Foundation.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor,
# Boston, MA  02110-1301, USA

RM = rm -f
CC = gcc
LD = ld
CFLAGS = -O3 -Wall -I.

TARGETS = mfc-daemon
OBJS = mfc-daemon.o

all: $(TARGETS)
work: $(OBJS)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

.c.o: 
	$(CC) $(CFLAGS) -c $< -o $@

install::
	install -T $(TARGETS) /usr/sbin/$(TARGETS)

startup::
	cp mfc-daemon.init /etc/init.d/mfc-daemon
	ln -s /etc/init.d/mfc-daemon /etc/rc2.d/S97mfc-daemon
	ln -s /etc/init.d/mfc-daemon /etc/rc3.d/S97mfc-daemon
	ln -s /etc/init.d/mfc-daemon /etc/rc5.d/S97mfc-daemon

uninstall::
	$(RM) /usr/sbin/$(TARGETS)
	$(RM) /etc/init.d/mfc-daemon
	$(RM) /etc/rc2.d/S97mfc-daemon
	$(RM) /etc/rc3.d/S97mfc-daemon
	$(RM) /etc/rc5.d/S97mfc-daemon

clean::
	$(RM) $(TARGETS) *.o *.d  *~

-include $(wildcard *.d) dummy

