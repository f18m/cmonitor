#
# Makefile to regenerate example data
# 
TOPDIR=..
EXAMPLE_JSON_FILES=\
	baremetal1.json.gz \
	baremetal2.json.gz \
	docker-collecting-baremetal-stats.json.gz \
	docker-userapp-with-embedded-collector.json.gz \
	docker-userapp-with-embedded-collector.json.gz \
	docker-userapp.json.gz
CMONITOR_COLLECTOR=$(TOPDIR)/collector/src/cmonitor_collector

SHELL:=/bin/bash 

all:
	for jsonExample in $(EXAMPLE_JSON_FILES); do \
		echo "--------------------------------------------------------" ; \
		../tools/chart/cmonitor_chart.py --input=$$jsonExample --verbose ; \
	done

clean:
	rm -f *.err docker-*/example-load.sh
	
distclean: clean
	rm -f *.html


#
# baremetal1 example:
#  example of graph generated with the performance stats collected from a physical (baremetal) server running Ubuntu 18.04
#

regen_baremetal1:
	@echo "Launching load simulator to have some CPU at 100% and so on..."
	./example-load.sh &
	@echo "Regenerating example JSON... this will take about 3*60secs."
	@$(CMONITOR_COLLECTOR) \
		--sampling-interval=10 \
		--num-samples=60 \
		--output-pretty \
		--output-filename baremetal1 \
		--foreground \
		--debug
	@pkill example-load
	@jq -e . baremetal1.json >/dev/null && ( echo "Valid JSON produced" ) || ( echo "INVALID JSON WAS PRODUCED !!!" ; exit 10 )
	@gzip -f baremetal1.json

#
# NOTE: baremetal2 is a real-world example and thus cannot be regenerated just like others
#



#
# docker_collecting_baremetal_stats: 
#  dockerized cmonitor monitoring the baremetal
#

regen_docker_collecting_baremetal_stats:
	make -C docker-collecting-baremetal-stats
	


#
# docker_userapp:
#  monitoring of a docker container from the baremetal
#
DOCKER_EXAMPLE_CPUS:=0,1
DOCKER_EXAMPLE_MEMORY_LIMIT:=100m
DOCKER_EXAMPLE_FOLDER:=$(shell readlink -f .)

regen_docker_userapp:
	docker stop userapp || true
	pkill example_load_re
	docker run \
		--detach \
		--interactive \
		--tty \
		--rm \
		--cpuset-cpus=$(DOCKER_EXAMPLE_CPUS) \
		--memory=$(DOCKER_EXAMPLE_MEMORY_LIMIT) \
		--volume=$(DOCKER_EXAMPLE_FOLDER):/perf \
		-P \
		--name userapp \
		redis:latest
	sleep 3
	for i in $$(seq 1 10); do \
		./example_load_redis.py userapp & \
		sleep 1 ; \
	done
	@echo "Now running cmonitor_collector on the baremetal to monitor the 'userapp' container:"
	# NOTE: we know that docker creates a cgroup always named docker/container-ID
	$(CMONITOR_COLLECTOR) \
		--sampling-interval=2 \
		--num-samples=60 \
		--collect=all_cgroup \
		--foreground \
		--output-filename=docker-userapp.json \
		--cgroup-name=docker/$$(docker ps --no-trunc -aqf name=userapp) \
		--custom-metadata="cmonitor_chart_name:userapp (Redis docker)"
	pkill example_load_re
	docker stop userapp
	gzip -f docker-userapp.json



#
# docker_userapp_with_embedded_collector:
#  dockerized cmonitor embedded inside a docker container
#

regen_docker_userapp_with_embedded_collector:
	make -C docker-userapp-with-embedded-collector
	

# docker helper:

attach_running_docker:
	docker exec -i -t cmonitor-example /bin/bash


#
# INFLUXDB & GRAFANA example:
#

reset_influxdb:
	# assuming you started in another console or in background the "influxd" daemon:
	influx -execute "DROP DATABASE cmonitor"
	influx -execute "CREATE DATABASE cmonitor"

show_cmonitor_data_in_influxdb:
	influx -execute 'SHOW SERIES' -database="cmonitor"
	influx -execute 'SELECT * FROM "stat_cpu0" LIMIT 20' -database="cmonitor"

regen_grafana_screenshots:
	@echo "Running InfluxDB inside a Docker"
	@docker stop influxdb || true
	rm -rf /tmp/influxdb/*   # remove data from previous runs
	docker pull influxdb
	docker run -d \
		--rm \
		-p 8086:8086 \
		-v /tmp/influxdb:/var/lib/influxdb \
		--name=influxdb \
		influxdb
	@echo "Running Grafana inside a Docker"
	@# Note that with --link it is possible to configure InfluxDB as source with link "http://influxdb:8086"
	@docker stop grafana || true
	docker pull grafana/grafana
	docker run -d \
		--rm \
		-p 3000:3000 \
		--link=influxdb \
		--name=grafana \
		grafana/grafana
	@# IMPORTANT: create the 'cmonitor' database before running the collector:
	#influx -execute "CREATE DATABASE cmonitor"
	sleep 10 # give time to influxDB to start!
	curl -i -XPOST http://localhost:8086/query --data-urlencode "q=CREATE DATABASE cmonitor"    # do not depend on "influx" CLI utility
	@echo "InfluxDB and Grafana dockers should be up and running... launching cmonitor to collect 10min of data"
	@$(CMONITOR_COLLECTOR) \
		--sampling-interval=2 --num-samples=300 \
		--remote-ip=localhost --remote-port=8086 \
		--output-filename=none \
		--debug \
		--foreground
	@echo "Now you can login from your browser at http://localhost:3000/login and create a Grafana dashboard to pick"
	@echo "data from the 'http://influxdb:8086' datasource, database 'cmonitor'..."
 
.PHONY: reset_influxdb show_cmonitor_data_in_influxdb regen_grafana_screenshots

