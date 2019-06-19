# as part of this example limit the CPUs where this docker can run to just 2 (the first 2 available)
DOCKER_EXAMPLE_CPUS:=0,1
DOCKER_EXAMPLE_MEMORY_LIMIT:=100m
DOCKER_EXAMPLE_FOLDER:=$(shell readlink -f ..)
HOSTNAME:=docker-userapp-with-embedded-collector

all: prepare_context
	@docker stop cmonitor-example || true
	@docker build -t cmonitor-image . && \
		echo "cmonitor docker example built successfully; now running it" && \
		docker run \
			--detach \
			--tty \
			--rm \
			--cpuset-cpus=$(DOCKER_EXAMPLE_CPUS) \
			--memory=$(DOCKER_EXAMPLE_MEMORY_LIMIT) \
			--volume=$(DOCKER_EXAMPLE_FOLDER):/perf \
			--hostname=$(HOSTNAME) \
			--name cmonitor-example \
			cmonitor-image && \
		echo "cmonitor docker successfully started: baremetal+container performance stats are collected in $(DOCKER_EXAMPLE_FOLDER)." && \
		echo "The docker will stop automatically after a while." && \
		docker logs -f cmonitor-example && \
		gzip -f ../$(HOSTNAME).json

prepare_context:
	@cp -fv ../example-load.sh .
	@cp -fv ../../src/cmonitor_collector .
	@if ldd ../../src/cmonitor_collector | grep libc.so.6 >/dev/null; then \
		echo "Found that cmonitor_collector is correctly compiled for Ubuntu apparently" ; \
	else \
		echo "cmonitor_collector needs to be built for x86_64 distro. Cannot proceed" ; \
		exit 2 ; \
	fi