/*
 * njmon_cgroups.cpp -- interacts with Linux control groups to allow
 *                      njmon to monitor only the CPU/memory/disk resources
 *                      that the current cgroup allows to use.
 * Developer: Francesco Montorsi.
 * (C) Copyright 2018 Francesco Montorsi

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <cstdint>
#include <cstdio>
#include <string>
#include <set>
#include <vector>
#include <sstream>
#include <fstream>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

// General tool to strip spaces from both ends:
std::string trim_string(const std::string& s) {
	if (s.empty())
		return s;
	std::string::size_type b = s.find_first_not_of(" \t\r\n");
	std::string::size_type e = s.find_last_not_of(" \t\r\n");
	if (b == std::string::npos)
		return ""; // No non-spaces
	return std::string(s, b, e - b + 1);
}

bool string2int(const std::string& str, int& result) {
	std::stringstream ss(str);
	if (!(ss >> result)) {
		return false;
	}
	return true;
}

bool string2int(const std::string& str, unsigned long long& result) {
	std::stringstream ss(str);
	if (!(ss >> result)) {
		return false;
	}
	return true;
}

bool file_exists(const char* filename) {
	struct stat buffer;
	int exist = stat(filename, &buffer);
	if (exist == 0)
		return true;
	else
		// -1
		return false;
}

template<typename T> std::string stl_container2string(const T& par, const std::string& delim) {
	// Fails to compile here if the function parameter is not an STL container.
	{
		typename T::const_iterator dummy = par.begin();
		(void) dummy;
	}

	if (par.empty())
		return "";

	std::ostringstream oss;
	for (typename T::const_iterator iter = par.begin(); iter != par.end(); ++iter)
		oss << *iter << delim;

	// remove last appended delimiter
	std::string ret = oss.str();
	if (ret.size() > delim.size()) {
		for (unsigned int i = 0; i < delim.size(); i++)
			ret.pop_back();
	}
	return ret;
}

std::vector<std::string> split_string_in_array(const std::string& str, char splitter) {
	std::vector<std::string> tokens;
	std::string trimmed = trim_string(str);
	std::stringstream ss(trimmed);
	std::string temp;

	while (getline(ss, temp, splitter)) // split into new "lines" based on character
		tokens.push_back(trim_string(temp));

	if (!trimmed.empty() && trimmed[trimmed.size() - 1] == splitter)
		// in this case we must forcefully push an empty token in returned array
		tokens.push_back("");

	return tokens;
}

bool parse_string_with_multiple_ranges(const std::string& data, std::vector<int>& result) {
	// here we support strings containing a combination of
	//  - plain numbers written in base 10
	//  - ranges: two numbers separed by "-"
	// separated by commas.
	// IMPORTANT: the output vector will be cleared at startup
	// IMPORTANT: the output vector will NOT be sorted
	// IMPORTANT: ranges A-B specified in the string will be present in the output vector as EXPANDED list

	std::vector<std::string> tokens = split_string_in_array(data, ',');

	result.clear();
	for (std::vector<std::string>::const_iterator it = tokens.begin(), end_it = tokens.end(); it != end_it; ++it) {
		const std::string& token = *it;
		std::vector<std::string> range = split_string_in_array(token, '-');
		if (range.size() == 1) {
			int res;
			if (!string2int(range[0], res))
				return false;

			result.push_back(res);
		} else if (range.size() == 2) {
			int start;
			if (!string2int(range[0], start))
				return false;
			int stop;
			if (!string2int(range[1], stop))
				return false;

			// expand the range in the output vector
			for (int i = start; i <= stop; i++)
				result.push_back(i);
		} else {
			return false;
		}
	}

	return true;
}

bool parse_string_with_multiple_ranges(const std::string& data, std::set<int>& result) {
	std::vector<int> tmpResult;
	if (!parse_string_with_multiple_ranges(data, tmpResult))
		return false;

	for (int i : tmpResult)
		result.insert(i);
	return true;
}

bool read_integers_with_range_validation(const std::string& filename, int lower_limit, int upper_limit,
		std::set<int>& cpus) {
	FILE* stream = fopen(filename.c_str(), "r");
	if (!stream)
		return false; // file does not exist, try next path

	char buffer[256] = "";
	fscanf(stream, "%255s", buffer);
	fclose(stream);

	if (!parse_string_with_multiple_ranges(buffer, cpus))
		return false; // invalid content format??

	std::set<int>::iterator cpuit = cpus.begin();
	while (cpuit != cpus.end()) {
		if (*cpuit >= lower_limit && *cpuit < upper_limit)
			cpuit++; // OK; the CPU index is valid
		else
			cpuit = cpus.erase(cpuit); // INVALID CPU index: remove it
	}

	return true;
}

/* static */
bool get_cgroup_path_for_pid(const std::string& cgroup_type, std::string& cgroup_pathOUT) {
	/*
	 *
	 * ABOUT /proc/%d/cgroup:
	 *   See http://man7.org/linux/man-pages/man7/cgroups.7.html, look for "/proc/[pid]/cgroup (since Linux 2.6.24)"
	 *   Each line is composed by:
	 *                     hierarchy-ID:controller-list:cgroup-path
	 *   The problem is that this file does not provide you the FULL cgroup path, which depends on where exactly that cgroup
	 *   has been mounted.
	 *
	 * ABOUT /proc/%d/mounts:
	 *   See http://man7.org/linux/man-pages/man5/fstab.5.html
	 *   Each line is composed by:
	 *                     fs_spec  fs_file  fs_vfstype  fs_mntops  fs_freq  fs_passno
	 *   We are interested into the lines that indicate a specific CGROUP TYPE mountpoint like:
	 *   under LXC:
	 *     cgroup /sys/fs/cgroup/cpuset/lxc/eva-allinone-correlator-main cgroup rw,nosuid,nodev,noexec,relatime,cpuset 0 0
	 *   under Docker:
	 *     cgroup /sys/fs/cgroup/cpuset cgroup ro,nosuid,nodev,noexec,relatime,cpuset 0 0
	 *   the second string fs_file (/sys/fs/cgroup/cpuset/lxc/eva-allinone-correlator-main or /sys/fs/cgroup/cpuset) tells you where
	 *   to find all the current value of that cgroup; the fourth string fs_mntops contains the indication of the cgroup type (e.g. cpuset)
	 */
	std::ifstream inputf("/proc/self/mounts");
	if (!inputf.is_open())
		return false; // cannot read the cgroup information!

	std::string line;
	while (std::getline(inputf, line)) {
		//cout << line << '\n';
		std::vector<std::string> tuple = split_string_in_array(line, ' ');
		if (tuple.size() != 6)
			return false; // invalid format

		std::string fs_spec = tuple[0];
		std::string fs_file = tuple[1];
		std::string fs_mntops = tuple[3];

		if (fs_spec == "cgroup" && fs_mntops.find(cgroup_type) != std::string::npos) {
			// found the right "cgroup type"

			if (fs_file.empty() || fs_file == "/") {
				// !!this process is NOT running under any cgroup!!
				cgroup_pathOUT = "";
				return false;
			} else {
				cgroup_pathOUT = fs_file;
				return true;
			}
		}
	}

	return false; // cgroup name not found
}

bool read_from_system_cpu_for_current_cgroup(std::string kernelPath, std::set<int>& cpus) {
	std::set<int> empty_set;
	return read_integers_with_range_validation(kernelPath + "/cpuset.cpus", 0, INT32_MAX, cpus);
}

uint64_t read_from_system_memory_limit_in_bytes_for_current_cgroup(std::string kernelPath) {
	FILE* stream = fopen((kernelPath + "/memory.limit_in_bytes").c_str(), "r");
	if (!stream)
		return 0; // file does not exist or not readable

	uint64_t ret = 0;
	fscanf(stream, "%lu", &ret);
	fclose(stream);

	return ret;
}

template std::string stl_container2string(const std::set<int>& par, const std::string& delim);

extern "C" {

// GLOBALS

int cgroup_found = 0;
uint64_t cgroup_memory_limit_bytes = 0;
std::string cgroup_memory_kernel_path;
std::string cgroup_cpuacct_kernel_path;
std::string cgroup_cpuset_kernel_path;
std::set<int> cgroup_cpus;


// STUFF DEFINED IN ORIGINAL NJMON SOURCE CODE:

extern int debug;
extern void psection(char *section);
extern void psub(char *resource);
extern void plong(char *name, long long value);
extern void pdouble(char *name, double value);
extern void pstring(char *name, char *value);
extern void psubend();
extern void psectionend();

// FUNCTIONS

void cgroup_init() {
	cgroup_found = 0;

	if (!get_cgroup_path_for_pid("memory", cgroup_memory_kernel_path))
		return;
	if (!get_cgroup_path_for_pid("cpu,cpuacct", cgroup_cpuacct_kernel_path))
		return;
	if (!get_cgroup_path_for_pid("cpuset", cgroup_cpuset_kernel_path))
		return;

	cgroup_memory_limit_bytes = read_from_system_memory_limit_in_bytes_for_current_cgroup(cgroup_memory_kernel_path);
	if (!read_from_system_cpu_for_current_cgroup(cgroup_cpuset_kernel_path, cgroup_cpus) || cgroup_memory_limit_bytes == 0)
		return;

	// cpuset and memory cgroups found:
	cgroup_found = 1;
	if (debug) {
		printf("Found cpuset cgroup limiting to CPUs: %s\n", stl_container2string(cgroup_cpus, ",").c_str());
		printf("Found memory cgroup limiting to Bytes: %lu\n", cgroup_memory_limit_bytes);
	}
}

void cgroup_config() {
	if (cgroup_found == 0)
		return;

	psection("cgroup_config");
	pstring("memory_path", &cgroup_memory_kernel_path[0]);
	pstring("cpuacct_path", &cgroup_cpuacct_kernel_path[0]);
	pstring("cpuset_path", &cgroup_cpuset_kernel_path[0]);

	std::string tmp = stl_container2string(cgroup_cpus, ",");
	pstring("cpus", &tmp[0]);
	plong("memory_limit_bytes", cgroup_memory_limit_bytes);
	psectionend();
}

int cgroup_is_allowed_cpu(int cpu) {
	if (cgroup_found == 0)
		return 1; // allowed
	return cgroup_cpus.find(cpu) != cgroup_cpus.end();
}

void cgroup_proc_memory() {
	if (cgroup_found == 0)
		return;

	// See
	//   https://lwn.net/Articles/529927/
	//   https://www.kernel.org/doc/Documentation/cgroup-v1/memory.txt
	//   https://www.kernel.org/doc/Documentation/cgroup-v2.txt
	int i, len;
	unsigned long long value;

	/* Static data */
	static FILE *fp = 0;
	static char line[8192];
	char label[512];

	if (fp == 0) {
		std::string path = cgroup_memory_kernel_path + "/memory.stat";
		if ((fp = fopen(path.c_str(), "r")) == NULL) {
			fp = 0;
			return;
		}
	} else
		rewind(fp);

	psection("cgroup_memory_stats");
	while (fgets(line, 1000, fp) != NULL) {
		len = strlen(line);
		if (strncmp(line, "total_", 6) != 0)
			continue; // skip NON-totals: collect only cgroup-total values

		for (i = 0; i < len; i++) {
			if (line[i] == '(')
				line[i] = '_';
			if (line[i] == ')')
				line[i] = ' ';
			if (line[i] == ':')
				line[i] = ' ';
			if (line[i] == '\n')
				line[i] = 0;
		}
		value = 0;
		sscanf(line, "%s %llu", label, &value);
		/*printf("read_data_numer(%s) |%s| |%s|=%lld\n", statname,label,numstr,atoll(numstr));*/
		plong(label, value);
	}
	psectionend();
}

void cgroup_proc_cpuacct(double elapsed_sec) {
	if (cgroup_found == 0)
		return;

	/* NOTE: newer distros have stats like
	 /sys/fs/cgroup/cpu,cpuacct/cpuacct.usage_percpu_sys
	 /sys/fs/cgroup/cpu,cpuacct/cpuacct.usage_percpu_user
	 but older ones (e.g. Centos7) have only:
	 /sys/fs/cgroup/cpu,cpuacct/cpuacct.usage_percpu
	 Here we try to handle both cases.

	 See:
	 https://www.kernel.org/doc/Documentation/cgroup-v1/cpuacct.txt
	 https://www.kernel.org/doc/Documentation/cgroup-v2.txt
	 */

#define MAX_LOGICAL_CPU 256
#define DELTA_LOGICAL(stat, i) ((double)(stat[i] - logical_cpu[i].stat)/(elapsed_sec * 1E9))

	unsigned long long user[MAX_LOGICAL_CPU];
	unsigned long long system[MAX_LOGICAL_CPU];
	double cpu_total_usage;

	/* Static data */
	static FILE *fp1 = 0, *fp2 = 0;
	static char line[8192];

	/* structure to recall previous values */
	struct utilisation {
		long long user;
		long long system;
	};
	static struct utilisation logical_cpu[MAX_LOGICAL_CPU] = { 0 };
	static unsigned int num_cpus = 0;

	char label[512];
	int i;

	if (fp1 == 0) {
		std::string path = cgroup_cpuacct_kernel_path + "/cpuacct.usage_percpu_sys";
		if (false) { //file_exists(path.c_str())) {

			// this system supports per-cpu system/user stats:

			if ((fp1 = fopen(path.c_str(), "r")) == NULL) {
				fp1 = 0;
				return;
			}
			path = cgroup_cpuacct_kernel_path + "/cpuacct.usage_percpu_user";
			if ((fp2 = fopen(path.c_str(), "r")) == NULL) {
				fp2 = 0;
				return;
			}

			if (debug)
				printf("Found cpuacct.usage_percpu_sys/user cgroups\n");
		} else {

			// just get the per-cpu total:

			path = cgroup_cpuacct_kernel_path + "/cpuacct.usage_percpu";
			if ((fp1 = fopen(path.c_str(), "r")) == NULL) {
				fp1 = 0;
				return;
			}

			if (debug)
				printf("Found only cpuacct.usage_percpu cgroup\n");

		}
	} else if (fp1 && fp2) {
		rewind(fp1);
		rewind(fp2);
		/*
		 fgets(line, 1000, fp);
		 std::vector<std::string> values = split_string_in_array(line, ' ');
		 i = 0;
		 psection("cgroup_cpuacct_stats");
		 for (auto val : values) {
		 if (string2int(val, &user[i])) {
		 plong(label, value);
		 }
		 }
		 psectionend();
		 */

	} else if (fp1) {
		rewind(fp1);

		fgets(line, 1000, fp1);
		std::vector<std::string> values = split_string_in_array(line, ' ');
		if (num_cpus == 0) {
			// first time we read the CPU stats
			num_cpus = values.size();
		} else {
			if (values.size() != num_cpus) {
				// error: we read a different number of CPUs compared to previous read
				fp1 = 0;
				num_cpus = 0;
				return;
			}
		}

		/*
		 * Docker stats does something like:

			func calculateCPUPercent(previousCPU, previousSystem uint64, v *types.StatsJSON) float64 {
			    var (
				cpuPercent = 0.0
				// calculate the change for the cpu usage of the container in between readings
				cpuDelta = float64(v.CPUStats.CPUUsage.TotalUsage) - float64(previousCPU)
				// calculate the change for the entire system between readings
				systemDelta = float64(v.CPUStats.SystemUsage) - float64(previousSystem)
			    )

			    if systemDelta > 0.0 && cpuDelta > 0.0 {
				cpuPercent = (cpuDelta / systemDelta) * float64(len(v.CPUStats.CPUUsage.PercpuUsage)) * 100.0
			    }
			    return cpuPercent
			}
		 */

		i = 0; cpu_total_usage = 0;
		for (auto val : values) {
			if (!string2int(val, user[i])) {
				return;
			}

//			cpu_total_usage += user[i];
			i++;
		}

		psection("cgroup_cpuacct_stats");
		for (i=0; i<num_cpus; i++) {
			// output JSON counter
			sprintf(label, "cpu%d", i);
			psub(label);

			double cpuDelta = DELTA_LOGICAL(user, i);
			pdouble("user", cpuDelta);
			psubend();

			logical_cpu[i].user = user[i];
		}
		psectionend();

	}
}

} // extern C section
