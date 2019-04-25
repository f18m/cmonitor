#
# Makefile to regenerate example data
# 

EXAMPLE_JSON_FILES=\
	baremetal1_20190413_1605.json \
	docker_collecting_baremetal_stats.json \
	docker_userapp_with_embedded_collector.json

all:
	for jsonExample in $(EXAMPLE_JSON_FILES); do \
		../njmonchart/njmon_chart.py $$jsonExample ; \
	done

regen_baremetal1:
	@echo "Regenerating example JSON... this will take about 3*60secs."
	@../src/njmon_collector --sampling-interval=3 --num-samples=60 --foreground --output-filename baremetal1_20190413_1605
	@jq . baremetal1_20190413_1605.json >/dev/null && ( echo "Valid JSON produced" ) || ( echo "INVALID JSON WAS PRODUCED !!!" ; exit 10 )
