#
# Makefile to regenerate example data
# 

EXAMPLE_JSON_FILES=\
	baremetal1.json \
	docker-ubuntu1804-userapp-with-embedded-collector.json

#	docker-collecting-baremetal-stats.json \

all:
	for jsonExample in $(EXAMPLE_JSON_FILES); do \
		../njmonchart/njmon_chart.py $$jsonExample ; \
	done

regen_baremetal1:
	@echo "Regenerating example JSON... this will take about 3*60secs."
	@../src/njmon_collector --sampling-interval=3 --num-samples=60 --foreground --output-filename baremetal1
	@jq . baremetal1.json >/dev/null && ( echo "Valid JSON produced" ) || ( echo "INVALID JSON WAS PRODUCED !!!" ; exit 10 )

clean:
	rm -f *.err *.html
	
