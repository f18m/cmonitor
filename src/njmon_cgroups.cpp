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

// General tool to strip spaces from both ends:
std::string trim_string(const std::string& s)
{
	if (s.empty())
		return s;
	std::string::size_type b = s.find_first_not_of(" \t\r\n");
	std::string::size_type e = s.find_last_not_of(" \t\r\n");
	if (b == std::string::npos)
		return ""; // No non-spaces
	return std::string(s, b, e - b + 1);
}

bool string2int(const std::string& str, int& result)
{
	std::stringstream ss(str);
	if (!(ss >> result))
	{
		return false;
	}
	return true;
}

template <typename T> std::string stl_container2string(const T& par, const std::string& delim)
{
	// Fails to compile here if the function parameter is not an STL container.
	{
		typename T::const_iterator dummy = par.begin();
		(void)dummy;
	}

	if (par.empty())
		return "";

	std::ostringstream oss;
	for (typename T::const_iterator iter = par.begin(); iter != par.end(); ++iter)
		oss << *iter << delim;

	// remove last appended delimiter
	std::string ret = oss.str();
	if (ret.size() > delim.size())
	{
		for (unsigned int i = 0; i < delim.size(); i++)
			ret.pop_back();
	}
	return ret;
}

std::vector<std::string> split_string_in_array(const std::string& str, char splitter)
{
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

bool parse_string_with_multiple_ranges(const std::string& data, std::vector<int>& result)
{
	// here we support strings containing a combination of
	//  - plain numbers written in base 10
	//  - ranges: two numbers separed by "-"
	// separated by commas.
	// IMPORTANT: the output vector will be cleared at startup
	// IMPORTANT: the output vector will NOT be sorted
	// IMPORTANT: ranges A-B specified in the string will be present in the output vector as EXPANDED list

	std::vector<std::string> tokens = split_string_in_array(data, ',');

	result.clear();
	for (std::vector<std::string>::const_iterator it = tokens.begin(), end_it = tokens.end(); it != end_it; ++it)
	{
		const std::string& token = *it;
		std::vector<std::string> range = split_string_in_array(token, '-');
		if (range.size() == 1)
		{
			int res;
			if (!string2int(range[0], res))
				return false;

			result.push_back(res);
		}
		else if (range.size() == 2)
		{
			int start;
			if (!string2int(range[0], start))
				return false;
			int stop;
			if (!string2int(range[1], stop))
				return false;

			// expand the range in the output vector
			for (int i = start; i <= stop; i++)
				result.push_back(i);
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool parse_string_with_multiple_ranges(const std::string& data, std::set<int>& result)
{
	std::vector<int> tmpResult;
	if (!parse_string_with_multiple_ranges(data, tmpResult))
		return false;

	for (int i : tmpResult)
		result.insert(i);
	return true;
}

bool read_integers_with_range_validation(const std::string& filename, int lower_limit, int upper_limit, std::set<int>& cpus)
{
	FILE* stream = fopen(filename.c_str(), "r");
	if (!stream)
		return false; // file does not exist, try next path

	char buffer[256] = "";
	fscanf(stream, "%255s", buffer);
	fclose(stream);

	if (!parse_string_with_multiple_ranges(buffer, cpus))
		return false; // invalid content format??

	std::set<int>::iterator cpuit = cpus.begin();
	while (cpuit != cpus.end())
	{
		if (*cpuit >= lower_limit && *cpuit < upper_limit)
			cpuit++; // OK; the CPU index is valid
		else
			cpuit = cpus.erase(cpuit); // INVALID CPU index: remove it
	}

	return true;
}

/* static */
bool get_cgroup_path_for_pid(const std::string& cgroup_type, std::string& cgroup_pathOUT)
{
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
	while (std::getline(inputf, line))
	{
		//cout << line << '\n';
		std::vector<std::string> tuple = split_string_in_array(line, ' ');
		if (tuple.size() != 6)
			return false; // invalid format

		std::string fs_spec = tuple[0];
		std::string fs_file = tuple[1];
		std::string fs_mntops = tuple[3];

		if (fs_spec == "cgroup" && fs_mntops.find(cgroup_type) != std::string::npos)
		{
			// found the right "cgroup type"

			if (fs_file.empty() || fs_file == "/")
			{
				// !!this process is NOT running under any cgroup!!
				cgroup_pathOUT = "";
				return false;
			}
			else
			{
				cgroup_pathOUT = fs_file;
				return true;
			}
		}
	}

	return false; // cgroup name not found
}

bool read_from_system_cpu_for_current_cgroup(std::set<int>& cpus)
{
	std::set<int> empty_set;
	std::string kernelPath;
	if (!get_cgroup_path_for_pid("cpuset", kernelPath))
		return false;

	return read_integers_with_range_validation(kernelPath + "/cpuset.cpus", 0, INT32_MAX, cpus);
}

uint64_t read_from_system_memory_limit_in_bytes_for_current_cgroup()
{
	std::string kernelPath;
	if (!get_cgroup_path_for_pid("memory", kernelPath))
		return 0;

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
int cgroup_found = 0;
std::set<int> cgroup_cpus;
extern int debug;

void cgroup_init() {
	cgroup_found = 0;

	uint64_t memLimit = 0;
	if (!read_from_system_cpu_for_current_cgroup(cgroup_cpus) ||
	   (memLimit = read_from_system_memory_limit_in_bytes_for_current_cgroup()) == 0)
		return;

	cgroup_cpus.erase(1); // FIXME FOR TEST

	// cpuset and memory cgroups found:
	cgroup_found = 1;
	if (debug) {
		printf("Found cpuset cgroup limiting to CPUs: %s\n", stl_container2string(cgroup_cpus, ",").c_str());
		printf("Found memory cgroup limiting to Bytes: %lu\n", memLimit);
	}

}

int cgroup_is_allowed_cpu(int cpu) {
	if (cgroup_found == 0)
		return 1; // allowed
	return cgroup_cpus.find(cpu) != cgroup_cpus.end();
}
}
