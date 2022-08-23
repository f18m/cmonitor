ROOT_DIR:=$(shell readlink -f ../..)
PYTHON_COMMON_CODE=$(ROOT_DIR)/tools/common-code

run:
	export PYTHONPATH=$(PYTHON_COMMON_CODE) ; \
		./cmonitor_launcher.py $(ARGS)

test:
	cd tests && \
		export PYTHONPATH=$(PYTHON_COMMON_CODE) && \
		pytest --capture=no -vv
