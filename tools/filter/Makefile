ROOT_DIR:=$(shell readlink -f ../..)
PYTHON_COMMON_CODE=$(ROOT_DIR)/tools/common-code

run:
	export PYTHONPATH=$(PYTHON_COMMON_CODE) ; \
		./cmonitor_filter.py $(ARGS)

# FIXME: rewrite these using Python unit test framework:
test1:
	@echo "------------------------------------------------ test1 ------------------------------------------------"
	@rm -f tests/result.json
	export PYTHONPATH=$(PYTHON_COMMON_CODE) ; \
		./cmonitor_filter.py --input=tests/docker-collecting-docker-stats.json --output="tests/result.json" --verbose \
			--start_timestamp="2022-01-18T00:02:50" --end_timestamp="2022-01-18T00:05:47.300"
	# the input JSON contains 180 samples:
	#   first one: "2022-01-18T00:02:47.897"
	#   last one: "2022-01-18T00:05:47.313"
	# so with filtering criteria we remove 2 samples from the start and 1 from the end --> 177
	@echo "Actual number of samples saved (expected is 177):"
	@jq '.samples | length' tests/result.json

test2:
	@echo "------------------------------------------------ test2 ------------------------------------------------"
	@rm -f tests/result.json
	export PYTHONPATH=$(PYTHON_COMMON_CODE) ; \
		./cmonitor_filter.py --input=tests/docker-collecting-docker-stats.json --output="tests/result.json" --verbose \
			--task_name="jemalloc_bg_thd"
	# the input JSON contains 5 threads in each of the 180 samples except the first sample ---> 5*179 = 895
	@echo "Input number of TASKS:"
	@jq '[ .samples[].cgroup_tasks[] ] | length' tests/docker-collecting-docker-stats.json
	# the input JSON contains 179 having "jemalloc_bg_thd" recordings inside; since we are filtering out every other
	# process/thread, we expect to count now only 179 tasks over 180 samples:
	@echo "Actual number of samples saved (expected is 180):"
	@jq '.samples | length' tests/result.json
	@echo "Actual number of TASKS saved (expected is 179):"
	@jq '[ .samples[].cgroup_tasks[] ] | length' tests/result.json

test: test1 test2