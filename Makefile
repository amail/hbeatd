CC=gcc
CFLAGS=-O3 -s -pthread -lc -lrt
LDFLAGS=--no-as-needed

hbeatd:
	$(CC) hbeatd.c $(CFLAGS) -Wl,$(LDFLAGS) -o hbeatd

install: hbeatd
	@mkdir -p /etc/hbeatd
	@echo "Installing hbeatd..."
	@cp hbeatd /usr/local/sbin/
	@cp hbeatd.conf /etc/hbeatd/
	@echo "Copying man pages hbeatd(7)..."
	@cp man/hbeatd.7.gz /usr/share/man/man7/
	@if [ -d "/etc/init.d" ]; then\
		echo "Installing init.d script...";\
		cp init.d/hbeatd /etc/init.d/;\
		ln -s /etc/init.d/hbeatd /etc/rc3.d/S95hbeatd;\
	fi
	@if [ -d "/etc/rc.d" ]; then\
		echo "Installing rc.d script...";\
		cp rc.d/hbeatd /etc/rc.d/;\
		echo 'hbeatd_enable="YES"' >> /etc/rc.conf;\
	fi
