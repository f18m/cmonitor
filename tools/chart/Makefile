ROOT_DIR:=$(shell readlink -f ../..)
PYTHON_COMMON_CODE=$(ROOT_DIR)/tools/common-code

run:
	export PYTHONPATH=$(PYTHON_COMMON_CODE) ; \
		./cmonitor_chart.py $(ARGS)
