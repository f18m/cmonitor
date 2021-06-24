# as part of this example limit the CPUs where this docker can run to just 1 (the first available)
DOCKER_EXAMPLE_CPUS:=0
DOCKER_EXAMPLE_MEMORY_LIMIT:=100m
DOCKER_EXAMPLE_FOLDER:=$(shell readlink -f ..)
HOSTNAME:=docker-collecting-baremetal-stats

all: prepare_context
	@docker stop collecting-baremetal-stats || true
	@docker build -t cmonitor-for-baremetal-image . && \
		echo "cmonitor docker example built successfully; now running it" && \
		docker run \
			--detach \
			--tty \
			--rm \
			--cpuset-cpus=$(DOCKER_EXAMPLE_CPUS) \
			--memory=$(DOCKER_EXAMPLE_MEMORY_LIMIT) \
			--volume=$(DOCKER_EXAMPLE_FOLDER):/perf \
			--hostname=$(HOSTNAME) \
			--name collecting-baremetal-stats \
			cmonitor-for-baremetal-image && \
		echo "cmonitor docker successfully started: baremetal performance stats are collected in $(DOCKER_EXAMPLE_FOLDER)." && \
		echo "The docker will stop automatically after about 10minutes." && \
		docker logs -f collecting-baremetal-stats && \
		gzip -f ../$(HOSTNAME).json

prepare_context:
	@cp -fv ../../collector/src/cmonitor_collector .
	@if ldd ../../collector/src/cmonitor_collector | grep libc.musl >/dev/null; then \
		echo "Found that cmonitor_collector is correctly compiled for Alpine distro" ; \
	else \
		echo "cmonitor_collector needs to be built for Alpine distro. Cannot proceed" ; \
		exit 2 ; \
	fi