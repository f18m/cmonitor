# as part of this example limit the CPUs where this docker can run to just 1 (the first available)
DOCKER_EXAMPLE_CPUS:=0
DOCKER_EXAMPLE_MEMORY_LIMIT:=100m

all:
	@docker build -t centos7-nmon-for-baremetal . && \
		echo "Njmon-cgroup-aware docker example built successfully; now running it" && \
		docker run -dit \
			--cpuset-cpus=$(DOCKER_EXAMPLE_CPUS) \
			--memory=$(DOCKER_EXAMPLE_MEMORY_LIMIT) \
			--volume=$(HOME):/perf \
			--hostname=docker_collecting_baremetal_stats \
			centos7-nmon-for-baremetal && \
		echo "Njmon-cgroup-aware docker successfully started: baremetal performance stats are collected in $(HOME)." && \
		echo "The docker will stop automatically after 3minutes."
