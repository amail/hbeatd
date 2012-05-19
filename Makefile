
hbeatd:
	gcc hbeatd.c -O3 -o hbeatd
	
install: hbeatd
	@if [ -d "/etc/hbeatd" ]; \
    	then \
		cp hbeatd /usr/local/sbin/;\
		echo "Installing hbeatd...";\
    	else \
        	mkdir /etc/hbeatd;\
        	cp hbeatd /usr/local/sbin/;\
		echo "Installing hbeatd...";\
	fi
	
