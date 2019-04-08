all:
	$(MAKE) -C src
	
clean:
	$(MAKE) -C src clean

install:
ifndef DESTDIR
	@echo "*** ERROR: please call this makefile supplying explicitly the DESTDIR variable"
	@exit 1
endif
	$(MAKE) -C src install DESTDIR=$(DESTDIR)
	
generate_patch:
	diff -U3 -w src-orig/njmon_linux_v22.c src/njmon_linux_v22.c > src-orig/cgroup.patch || true

generate_rpm:
	rpmbuild -bb spec/njmon-cgroup-aware.spec
