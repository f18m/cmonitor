#
# Makefile to regenerate example data
# 

EXAMPLE_JSON_FILES=\
	baremetal1.json \
	baremetal2.json \
	docker-centos7-collecting-baremetal-stats.json \
	docker-centos7-userapp-with-embedded-collector.json \
	docker-ubuntu1804-userapp-with-embedded-collector.json

all:
	for jsonExample in $(EXAMPLE_JSON_FILES); do \
		echo "--------------------------------------------------------" ; \
		../njmonchart/njmon_chart.py $$jsonExample ; \
	done

clean:
	rm -f *.err docker-*/example-load.sh
	
distclean: clean
	rm -f *.html
	

#
# baremetal1 example:
#

regen_baremetal1:
	@echo "Regenerating example JSON... this will take about 3*60secs."
	@../src/njmon_collector --sampling-interval=3 --num-samples=60 --foreground --output-filename baremetal1
	@jq -e . baremetal1.json >/dev/null && ( echo "Valid JSON produced" ) || ( echo "INVALID JSON WAS PRODUCED !!!" ; exit 10 )


#
# INFLUXDB & GRAFANA
#

reset_influxdb:
	# assuming you started in another console or in background the "influxd" daemon:
	influx -execute "DROP DATABASE njmon"
	influx -execute "CREATE DATABASE njmon"

show_njmon_data_in_influxdb:
	influx -execute 'SHOW SERIES' -database="njmon"
	influx -execute 'SELECT * FROM "stat_cpu0" LIMIT 20' -database="njmon"

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
	@# IMPORTANT: create the 'njmon' database before running the collector:
	#influx -execute "CREATE DATABASE njmon"
	sleep 10 # give time to influxDB to start!
	curl -i -XPOST http://localhost:8086/query --data-urlencode "q=CREATE DATABASE njmon"    # do not depend on "influx" CLI utility
	@echo "InfluxDB and Grafana dockers should be up and running... launching njmon to collect 10min of data"
	@../src/njmon_collector \
		--sampling-interval=2 --num-samples=300 \
		--remote-ip=localhost --remote-port=8086 \
		--output-filename=none \
		--debug \
		--foreground
	@echo "Now you can login from your browser at http://localhost:3000/login and create a Grafana dashboard to pick"
	@echo "data from the 'http://influxdb:8086' datasource, database 'njmon'..."
 
.PHONY: reset_influxdb show_njmon_data_in_influxdb regen_grafana_screenshots

