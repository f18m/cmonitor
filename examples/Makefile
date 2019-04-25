#
# Makefile to regenerate example data
# 

EXAMPLE_JSON_FILES=\
	baremetal1_20190413_1605.json

all:
	for jsonExample in $(EXAMPLE_JSON_FILES); do \
		../njmonchart/njmon_chart.py $$jsonExample ; \
	done

regen_baremetal1:
	../src/njmon_collector --sampling-interval=3 --num-samples=120 --output-filename baremetal1_20190413_1605
