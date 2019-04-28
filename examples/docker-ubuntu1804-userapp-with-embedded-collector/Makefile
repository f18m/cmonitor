# as part of this example limit the CPUs where this docker can run to just 2 (the first 2 available)
DOCKER_EXAMPLE_CPUS:=0,1
DOCKER_EXAMPLE_MEMORY_LIMIT:=100m
DOCKER_EXAMPLE_FOLDER:=$(shell readlink -f ..)

all:
	@cp -fv ../../src/njmon_collector .
	@cp -fv ../example-load.sh .
	@docker build -t ubuntu1804-nmon-image . && \
		echo "Njmon-cgroup-aware docker example built successfully; now running it" && \
		docker run \
			--detach \
			--tty \
			--rm \
			--cpuset-cpus=$(DOCKER_EXAMPLE_CPUS) \
			--memory=$(DOCKER_EXAMPLE_MEMORY_LIMIT) \
			--volume=$(DOCKER_EXAMPLE_FOLDER):/perf \
			--hostname=docker-ubuntu1804-userapp-with-embedded-collector \
			--name ubuntu1804-nmon-example \
			ubuntu1804-nmon-image && \
		echo "Njmon-cgroup-aware docker successfully started: baremetal+container performance stats are collected in $(DOCKER_EXAMPLE_FOLDER)." && \
		echo "The docker will stop automatically after a while." && \
		docker logs -f ubuntu1804-nmon-example
