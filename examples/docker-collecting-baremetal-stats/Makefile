# as part of this example limit the CPUs where this docker can run to just 1 (the first available)
DOCKER_EXAMPLE_CPUS:=0

all:
	docker build -t centos7-nmon-example . && \
		echo "Njmon-cgroup-aware docker example built successfully; now running it" && \
		docker run -dit --cpuset-cpus=$(DOCKER_EXAMPLE_CPUS) --volume=$(HOME):/perf centos7-nmon-example && \
		echo "Njmon-cgroup-aware docker successfully started: baremetal performance stats are collected in $(HOME)"
