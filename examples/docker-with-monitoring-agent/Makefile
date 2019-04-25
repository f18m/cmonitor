# as part of this example limit the CPUs where this docker can run to just 2 (the first 2 available)
DOCKER_EXAMPLE_CPUS:=0,1
DOCKER_EXAMPLE_MEMORY_LIMIT:=100m

all:
	docker build -t centos7-nmon-example . && \
		echo "Njmon-cgroup-aware docker example built successfully; now running it" && \
		docker run -dit \
			--cpuset-cpus=$(DOCKER_EXAMPLE_CPUS) \
			--memory=$(DOCKER_EXAMPLE_MEMORY_LIMIT) \
			--volume=$(HOME):/perf \
			centos7-nmon-example && \
		echo "Njmon-cgroup-aware docker successfully started: baremetal+container performance stats are collected in $(HOME)." && \
		echo "The docker will stop automatically after a while."
