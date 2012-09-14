
hbeatd:
	gcc hbeatd.c -O3 -o hbeatd

install: hbeatd
	@if [ -d "/etc/hbeatd" ]; \
   	then \
		cp hbeatd /usr/local/sbin/;\
		echo "Installing hbeatd...";\
		echo "Copying man pages hbeatd(7)...";\
		cp man/hbeatd.7.gz /usr/share/man/man7/;\
   	else \
       	mkdir /etc/hbeatd;\
       	cp hbeatd /usr/local/sbin/;\
		echo "Installing hbeatd...";\
		echo "Copying man pages hbeatd(7)...";\
		cp man/hbeatd.7.gz /usr/share/man/man7/;\
	fi\

