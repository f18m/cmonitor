# as part of this example limit the CPUs where this docker can run to just 1 (the first available)
DOCKER_EXAMPLE_CPUS:=0
DOCKER_EXAMPLE_MEMORY_LIMIT:=100m
DOCKER_EXAMPLE_FOLDER:=$(shell readlink -f ..)

all:
	@docker build -t centos7-nmon-for-baremetal-image . && \
		echo "Njmon-cgroup-aware docker example built successfully; now running it" && \
		docker run \
			--detach \
			--tty \
			--rm \
			--cpuset-cpus=$(DOCKER_EXAMPLE_CPUS) \
			--memory=$(DOCKER_EXAMPLE_MEMORY_LIMIT) \
			--volume=$(DOCKER_EXAMPLE_FOLDER):/perf \
			--hostname=docker-centos7-collecting-baremetal-stats \
			--name centos7-collecting-baremetal-stats \
			centos7-nmon-for-baremetal-image && \
		echo "Njmon-cgroup-aware docker successfully started: baremetal performance stats are collected in $(DOCKER_EXAMPLE_FOLDER)." && \
		echo "The docker will stop automatically after 3minutes." && \
		docker logs -f centos7-collecting-baremetal-stats
