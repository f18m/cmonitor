all:
	$(MAKE) -C src
	
clean:
	$(MAKE) -C src clean
	
generate_patch:
	diff -U3 -w src-orig/njmon_linux_v22.c src/njmon_linux_v22.c > src-orig/cgroup.patch || true
