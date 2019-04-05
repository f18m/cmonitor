/*
 * njmon.c -- collects Linux performance data and generates JSON format data.
 * Developer: Nigel Griffiths.
 * (C) Copyright 2018 Nigel Griffiths

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
/* To do list: Top processes, NFS lshw and Fibre Channel */
/* Compile example: cc -O4 njmon_linux njmon_linux.c */

/* njmon_colltor version needs to match the version in the njmon_collector.c */
#define COLLECTOR_VERSION "12"

/* njmon version */
#define VERSION "22@31/03/2019" /* year month day */
char version[] = VERSION;
static char *SccsId = "njmon for Linux " VERSION;
char *command;

#include <ctype.h>
#include <fcntl.h>
#include <mntent.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/vfs.h>
#include <time.h>
#include <unistd.h>

#include <memory.h>

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>

#define PRINT_FALSE 0
#define PRINT_TRUE 1

#define FUNCTION_START \
  if (debug) fprintf(stderr, "%s called line %d\n", __func__, __LINE__);
#define DEBUG if (debug)

int debug = 0;
#define ONE_LEVEL 1
#define MULTI_LEVEL 9
int mode = MULTI_LEVEL;
int oldmode = 0;

/* collect stats on the metrix */
int njmon_stats = 0;
int njmon_sections = 0;
int njmon_subsections = 0;
int njmon_string = 0;
int njmon_long = 0;
int njmon_double = 0;
int njmon_hex = 0;

/* Output JSON test buffering to ensure ist a single write and allow EOL comma
 * removal */
char *output;
long output_size = 0;
long output_char = 0;
char *nullstring = "";
long level = 0;

char hostname[256] = { 0 };
char shorthostname[256] = { 0 };

void interrupt(int signum) {
	switch (signum) {
	case SIGUSR1:
	case SIGUSR2:
		fflush(NULL);
		exit(0);
		break;
	}
}

int sockfd = 1; /*default is stdout, only changed if we are using a remote socket */

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * - */
/*   p functions to generate JSON output
 *    psection(name) and psectionend()
 *       Adds
 *           "name": {
 *
 *           }
 *
 *    psub(name) and psubend()
 *       similar to psection/psectionend but one level deeper
 *
 *    pstring(name,"abc")
 *    plong(name, 1234)
 *    pdouble(name, 1234.546)
 *    phex(name, hedadecimal number)
 *    praw(name) for other stuff in a raw format
 *       add "name": data,
 *
 *    the JSON is appended to the buffer "output" so
 *        we can remove the trailing "," before we close the entry with a "}"
 *        we can write the whole record in a single write (push()) to help down
 * stream tools
 */

#ifndef NOREMOTE
/* below incldes are for socket handling */
#include <arpa/inet.h>
#include <netinet/in.h>

void pexit(char *msg) {
	perror(msg);
	exit(1);
}

int en[94] = { 8, 85, 70, 53, 93, 72, 61, 1, 41, 36, 49, 92, 44, 42, 25, 58, 81,
		15, 57, 10, 54, 60, 12, 45, 43, 91, 22, 86, 65, 9, 27, 18, 37, 39, 2,
		68, 46, 71, 6, 79, 76, 84, 59, 75, 82, 4, 48, 55, 64, 3, 7, 56, 40, 73,
		77, 69, 88, 13, 35, 11, 66, 26, 52, 78, 28, 89, 51, 0, 30, 50, 34, 5,
		32, 21, 14, 38, 19, 29, 24, 33, 47, 31, 80, 16, 83, 90, 67, 23, 20, 17,
		74, 62, 87, 63 };

int de[94] = { 67, 7, 34, 49, 45, 71, 38, 50, 0, 29, 19, 59, 22, 57, 74, 17, 83,
		89, 31, 76, 88, 73, 26, 87, 78, 14, 61, 30, 64, 77, 68, 81, 72, 79, 70,
		58, 9, 32, 75, 33, 52, 8, 13, 24, 12, 23, 36, 80, 46, 10, 69, 66, 62, 3,
		20, 47, 51, 18, 15, 42, 21, 6, 91, 93, 48, 28, 60, 86, 35, 55, 2, 37, 5,
		53, 90, 43, 40, 54, 63, 39, 82, 16, 44, 84, 41, 1, 27, 92, 56, 65, 85,
		25, 11, 4 };

void mixup(char *s) {
	int i;
	for (i = 0; s[i]; i++) {
		if (s[i] <= ' ')
			continue;
		if (s[i] > '~')
			continue;
		s[i] = en[s[i] - 33] + 33;
	}
}

void unmix(char *s) {
	int i;
	for (i = 0; s[i]; i++) {
		if (s[i] <= ' ')
			continue;
		if (s[i] > '~')
			continue;
		s[i] = de[s[i] - 33] + 33;
	}
}

void create_socket(char *ip_address, long port, char *hostname, char *utc,
		char *secretstr) {
	int i;
	char buffer[8196];
	static struct sockaddr_in serv_addr;

	FUNCTION_START;
	DEBUG
		printf("socket: trying to connect to %s:%ld\n", ip_address, port);
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		pexit("njmon:socket() call failed");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(ip_address);
	serv_addr.sin_port = htons(port);

	/* Connect tot he socket offered by the web server */
	if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		pexit("njmon: connect() call failed");

	/* Now the sockfd can be used to communicate to the server the GET request */
	sprintf(buffer, "preamble-here njmon %s %s %s %s postamble-here", hostname,
			utc, secretstr, COLLECTOR_VERSION);
	DEBUG
		printf("hello string=\"%s\"\n", buffer);
	mixup(buffer);
	if (write(sockfd, buffer, strlen(buffer)) < 0)
		pexit("njmon: write() to socket failed");
}
#endif /* NOREMOTE */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * - */

void get_hostname() {
	int i;

	FUNCTION_START;
	if (hostname[0] != 0)
		return;
	if (gethostname(hostname, sizeof(hostname)) != 0)
		strcpy(hostname, "unknown-hotname");

	strcpy(shorthostname, hostname);
	for (i = 0; i < strlen(shorthostname); i++)
		if (shorthostname[i] == '.') {
			shorthostname[i] = 0;
			break;
		}
}

void remove_ending_comma_if_any() {
	if (output[output_char - 2] == ',') {
		output[output_char - 2] = '\n';
		output_char--;
	}
}

void buffer_check() {
	long size;
	if (output_char > (long) (output_size * 0.95)) { /* within 5% of the end */
		size = output_size + (1024 * 1024); /* add another MB */
		output = realloc((void *) output, size);
		output_size = size;
	}
}

void praw(char *string) {
	output_char += sprintf(&output[output_char], "%s", string);
}

void pstart() {
	DEBUG
		praw("START");
	praw("{\n");
}

void pfinish() {
	DEBUG
		praw("FINISH");
	remove_ending_comma_if_any();
	praw("}\n");
}

void psample() {
	DEBUG
		praw("SAMPLE");
	praw("  {\n"); /* start of sample */
}

void psampleend(int comma_needed) {
	DEBUG
		praw("SAMPLEEND");
	remove_ending_comma_if_any();
	if (comma_needed)
		praw("  }\n"); /* end of sample */
	else
		praw("  },\n"); /* end of sample more to come */
}

char *saved_section;
char *saved_resource;
long saved_level = 1;

void indent() {
	int i;
	DEBUG
		praw("INDENT");

	if (mode == ONE_LEVEL)
		saved_level = 2;

	for (i = 0; i < saved_level; i++)
		praw("     ");
}

void psection(char *section) {
	buffer_check();
	njmon_sections++;
	saved_section = section;
	if (mode == MULTI_LEVEL) {
		indent();
		output_char += sprintf(&output[output_char], "\"%s\": {\n", section);
	}
	saved_level++;
}

void psub(char *resource) {
	buffer_check();
	njmon_subsections++;
	saved_resource = resource;
	saved_level++;
	if (mode == MULTI_LEVEL) {
		indent();
		output_char += sprintf(&output[output_char], "\"%s\": {\n", resource);
	}
}

void psubend() {
	saved_resource = NULL;
	if (mode == MULTI_LEVEL) {
		remove_ending_comma_if_any();
		indent();
		praw("},\n");
	}
	saved_level--;
}

void psectionend() {
	saved_section = NULL;
	saved_resource = NULL;
	saved_level--;
	if (mode == MULTI_LEVEL) {
		remove_ending_comma_if_any();
		indent();
		praw("},\n");
	}
}

void phex(char *name, long long value) {
	indent();
	njmon_hex++;
	if (mode == ONE_LEVEL) {
		output_char += sprintf(&output[output_char],
				"\"%s%s%s_%s\": \"0x%08llx\",\n", saved_section,
				saved_resource == NULL ? "" : "_",
				saved_resource == NULL ? "" : saved_resource, name, value);
	} else {
		output_char += sprintf(&output[output_char], "\"%s\": \"0x%08llx\",\n",
				name, value);
	}
	DEBUG
		printf("plong(%s,%lld) count=%ld\n", name, value, output_char);
}

void plong(char *name, long long value) {
	indent();
	njmon_long++;
	if (mode == ONE_LEVEL) {
		output_char += sprintf(&output[output_char], "\"%s%s%s_%s\": %lld,\n",
				saved_section, saved_resource == NULL ? "" : "_",
				saved_resource == NULL ? "" : saved_resource, name, value);
	} else {
		output_char += sprintf(&output[output_char], "\"%s\": %lld,\n", name,
				value);
	}
	DEBUG
		printf("plong(%s,%lld) count=%ld\n", name, value, output_char);
}

void pdouble(char *name, double value) {
	indent();
	njmon_double++;
	if (mode == ONE_LEVEL) {
		output_char += sprintf(&output[output_char], "\"%s%s%s_%s\": %.3f,\n",
				saved_section, saved_resource == NULL ? "" : "_",
				saved_resource == NULL ? "" : saved_resource, name, value);
	} else {
		output_char += sprintf(&output[output_char], "\"%s\": %.3f,\n", name,
				value);
	}
	DEBUG
		printf("pdouble(%s,%.1f) count=%ld\n", name, value, output_char);
}

void pstats() {
	psection("njmon_stats");
	plong("section", njmon_sections);
	plong("subsections", njmon_subsections);
	plong("string", njmon_string);
	plong("long", njmon_long);
	plong("double", njmon_double);
	plong("hex", njmon_hex);
	psectionend("njmon_stats");
}

void pstring(char *name, char *value) {
	buffer_check();
	njmon_string++;
	indent();
	if (mode == ONE_LEVEL) {
		output_char += sprintf(&output[output_char], "\"%s%s%s_%s\": \"%s\",\n",
				saved_section, saved_resource == NULL ? "" : "_",
				saved_resource == NULL ? "" : saved_resource, name, value);
	} else {
		output_char += sprintf(&output[output_char], "\"%s\": \"%s\",\n", name,
				value);
	}
	DEBUG
		printf("pstring(%s,%s) count=%ld\n", name, value, output_char);
}

void push() {
	FUNCTION_START;
	buffer_check();
	DEBUG
		printf("XXX size=%ld\n", output_char);
	if (write(sockfd, output, output_char) < 0) {
		/* if stdout failed there is not must we can do so stop */
		perror("njmon write to stdout failed, stopping now.");
		exit(99);
	}

	fflush(NULL); /* force I/O output now */
	DEBUG
		printf("YYY size=%ld\n", output_char);
	output[0] = 0;
	output_char = 0;
}

int error(char *buf) {
	printf("ERROR: %s\n", buf);
	exit(1);
}

time_t timer; /* used to work out the time details*/
struct tm *tim; /* used to work out the local hour/min/second */

void get_time() {
	timer = time(0);
}

void get_localtime() {
	tim = localtime(&timer);
	tim->tm_year += 1900; /* read localtime() manual page!! */
	tim->tm_mon += 1; /* because it is 0 to 11 */
}

void get_utc() {
	tim = gmtime(&timer);
	tim->tm_year += 1900; /* read gmtime() manual page!! */
	tim->tm_mon += 1; /* because it is 0 to 11 */
}

void date_time(long seconds, long loop, long maxloops) {
	char buffer[256];

	FUNCTION_START;
	/* This is ISO 8601 datatime string format - ughly but get over it! :-) */
	get_time();
	get_localtime();
	psection("timestamp");
	sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d", tim->tm_year, tim->tm_mon,
			tim->tm_mday, tim->tm_hour, tim->tm_min, tim->tm_sec);
	pstring("datetime", buffer);
	get_utc();
	sprintf(buffer, "%04d-%02d-%02dT%02d:%02d:%02d", tim->tm_year, tim->tm_mon,
			tim->tm_mday, tim->tm_hour, tim->tm_min, tim->tm_sec);
	pstring("UTC", buffer);
	plong("snapshot_seconds", seconds);
	plong("snapshot_maxloops", maxloops);
	plong("snapshot_loop", loop);
	psectionend();
}

/* - - - - - gpfs - - - - */
#ifndef NOGPFS
int gpfs_na = 0; /* Not available, switches off any futher GPFS stats collection
 attempts */
char ip[1024]; /* IP address */
char nn[1024]; /* Node name (I think) */

/* this is the io_s stats data structure */
/* _io_s_ _n_ 192.168.50.20 _nn_ ems1-hs _rc_ 0 _t_ 1548346611 _tu_ 65624 _br_ 0
 * _bw_ 0 _oc_ 1 _cc_ 1 _rdc_ 0 _wc_ 0 _dir_ 1 _iu_ 0 */
struct gpfs_io {
	long rc;
	long t;
	long tu;
	long br;
	long bw;
	long oc;
	long cc;
	long rdc;
	long wc;
	long dir;
	long iu;
} gpfs_io_prev, gpfs_io_curr;

/* this is the fs_io_s stats data structure */
/*_fs_io_s_ _n_ 192.168.50.20 _nn_ ems1-hs _rc_ 0 _t_ 1548519197 _tu_ 560916
 * _cl_ SBANK_ESS.gpfs.net _fs_ cesroot _d_ 4 _br_ 224331 _bw_ 225922 _oc_ 63
 * _cc_ 58 _rdc_ 35 _wc_ 34 _dir_ 2 _iu_ 14 */

#define MAX_FS 64

struct gpfs_fs { /* this is the fs_io_s stats data structure */
	long rc;
	long t;
	long tu;
	char cl[512];
	char fs[512];
	long d;
	long br;
	long bw;
	long oc;
	long cc;
	long rdc;
	long wc;
	long dir;
	long iu;
} gpfs_fs_prev[MAX_FS], gpfs_fs_curr[MAX_FS];

int outfd[2];
int infd[2];
int pid = -99;

int gpfs_grab() {
	int i = 0;
	int index = 0;
	int records = 0;
	int ret;
	int count;
	char b[1024];
	char buffer[2048];

	FUNCTION_START;
	if (gpfs_na)
		return -1;
	/* first the total I/O stats */
	count = write(outfd[1], "io_s\n", strlen("io_s\n"));
	if (count != strlen("io_s\n")) {
		gpfs_na = 1;
		return 0;
	}
	count = read(infd[0], buffer, sizeof(buffer) - 1);
	if (count >= 0) {
		buffer[count] = 0;
		/*                                       1      2      3      4      5
		 * 6      7      8      9      10     11 */
		ret = sscanf(buffer,
				"%s %s %s %s %s %s %ld %s %ld %s %ld %s %ld %s %ld %s %ld %s "
						"%ld %s %ld %s %ld %s %ld %s %ld", b, b, &ip[0], b,
				&nn[0], b, &gpfs_io_curr.rc, b, &gpfs_io_curr.t, b,
				&gpfs_io_curr.tu, b, &gpfs_io_curr.br, b, &gpfs_io_curr.bw, b,
				&gpfs_io_curr.oc, b, &gpfs_io_curr.cc, b, &gpfs_io_curr.rdc, b,
				&gpfs_io_curr.wc, b, &gpfs_io_curr.dir, b, &gpfs_io_curr.iu);
	} else {
		gpfs_na = 1;
	}

	/* second the 1 or more filesystem  I/O stats */
	index = 0;
	count = write(outfd[1], "fs_io_s\n", strlen("fs_io_s\n"));
	if (count != strlen("fs_io_s\n")) {
		gpfs_na = 1;
		return 0;
	}
	count = read(infd[0], buffer, sizeof(buffer) - 1);
	buffer[count] = 0; /*ensure a zero string ending */
#ifdef TEST
	{
		/* fake a second filesystem */
		int len;
		len = strlen(buffer);
		strncpy(&buffer[len], buffer, len);
		count = strlen(buffer);
	}
#endif
	if (count >= 0) {
		for (i = 0; i < count; i++) {
			if (buffer[i] == '\n')
				records++;
		}
		if (records > 64)
			records = 64;
		for (i = 0; i < records; i++) {
			/*_fs_io_s_ _n_ 192.168.50.20 _nn_ ems1-hs _rc_ 0 _t_ 1548519197 _tu_
			 * 560916 _cl_ SBANK_ESS.gpfs.net _fs_ cesroot _d_ 4 _br_ 224331 _bw_
			 * 225922 _oc_ 63 _cc_ 58 _rdc_ 35 _wc_ 34 _dir_ 2 _iu_ 14 */
			/*                                       1      2      3      4      5
			 * 6      7      8      9      10     11 */
			ret =
					sscanf(&buffer[index],
							"%s %s %s %s %s %s %ld %s %ld %s %ld %s %s %s %s %s %ld %s %ld %s "
									"%ld %s %ld %s %ld %s %ld %s %ld %s %ld %s %ld",
							b, b, &ip[0], b, &nn[0], b, &gpfs_fs_curr[i].rc, b,
							&gpfs_fs_curr[i].t, b, &gpfs_fs_curr[i].tu, b,
							&gpfs_fs_curr[i].cl[0], b, &gpfs_fs_curr[i].fs[0],
							b, &gpfs_fs_curr[i].d, b, &gpfs_fs_curr[i].br, b,
							&gpfs_fs_curr[i].bw, b, &gpfs_fs_curr[i].oc, b,
							&gpfs_fs_curr[i].cc, b, &gpfs_fs_curr[i].rdc, b,
							&gpfs_fs_curr[i].wc, b, &gpfs_fs_curr[i].dir, b,
							&gpfs_fs_curr[i].iu);
			for (; index < count; index++) {
				if (buffer[index] == '\n') { /* find newline = terminating the current record */
					index++; /* move to after the newline */
					break;
				}
			}
			if (index == count)
				break;
		}
	} else {
		gpfs_na = 1;
	}
	return records;
}

void gpfs_init() {
	int filesystems = 0;
	struct stat sb; /* to check if mmpmon is executable and gpfs is installed */

	/* call shell script to start mmpmon binary */
	char *argv[] = { "/usr/lpp/mmfs/bin/mmksh", "-c",
			"/usr/lpp/mmfs/bin/mmpmon -s -p", 0 }; /* */

	/* Alternative: direct start of mmpmon */
	/* char *argv[]={ "/usr/lpp/mmfs/bin/tspmon", "1000", "1", "1", "0", "0",
	 * "60", "0", "/var/mmfs/mmpmon/mmpmonSocket", 0}; /* */

	FUNCTION_START;
	if (getuid() != 0)
		gpfs_na = 1; /* not available = mmpmon required root user */

	if (stat(argv[0], &sb) != 0)
		gpfs_na = 1; /* not available = no file */

	if (!(sb.st_mode & S_IXUSR))
		gpfs_na = 1; /* not available = not executable */

	if (gpfs_na)
		return;

	if (pipe(outfd) != 0) { /* Where the parent is going to write outfd[1] to
	 child input outfd[0] */
		gpfs_na = 1;
		return;
	}
	if (pipe(infd) != 0) { /* From where parent is going to read  infd[0] from
	 child output infd[1] */
		gpfs_na = 1;
		return;
	}
	DEBUG
		fprintf(stderr, "forking to run GPFS mmpmon command\n");
	if ((pid = fork()) == 0) {
		/* child process */
		close(0);
		dup2(outfd[0], 0);

		close(1);
		dup2(infd[1], 1);

		/* Not required for the child */
		close(outfd[0]);
		close(outfd[1]);
		close(infd[0]);
		close(infd[1]);

		execv(argv[0], argv);
		/* never returns */
	} else {
		/* parent process */
		close(outfd[0]); /* These are being used by the child */
		close(infd[1]);
		filesystems = gpfs_grab();
		/* copy to the previous records for next time */
		memcpy((void *) &gpfs_io_prev, (void *) &gpfs_io_curr,
				sizeof(struct gpfs_io));
		memcpy((void *) &gpfs_fs_prev[0], (void *) &gpfs_fs_curr[0],
				sizeof(struct gpfs_fs) * filesystems);
	}
}

void gpfs_data(double elapsed) {
	char buffer[10000];
	int records;
	int i;
	int ret;

	FUNCTION_START;
	if (gpfs_na)
		return;

	records = gpfs_grab(&gpfs_io_curr, &gpfs_fs_curr);

#define DELTA_GPFS(xxx) \
  ((double)(gpfs_io_curr.xxx - gpfs_io_prev.xxx) / elapsed)

	psection("gpfs_io_total");
	pstring("node", ip);
	pstring("name", nn);
	plong("rc", gpfs_io_curr.rc); /* status */
	plong("time", gpfs_io_curr.t); /* epoc seconds */
	plong("tu", DELTA_GPFS(tu));
	plong("readbytes", DELTA_GPFS(br));
	plong("writebytes", DELTA_GPFS(bw));
	plong("open", DELTA_GPFS(oc));
	plong("close", DELTA_GPFS(cc));
	plong("reads", DELTA_GPFS(rdc));
	plong("writes", DELTA_GPFS(wc));
	plong("directorylookup", DELTA_GPFS(dir));
	plong("inodeupdate", DELTA_GPFS(iu));
	psectionend();

	memcpy((void *) &gpfs_io_prev, (void *) &gpfs_io_curr,
			sizeof(struct gpfs_io));

#define DELTA_GPFSFS(xxx) \
  ((double)(gpfs_fs_curr[i].xxx - gpfs_fs_prev[i].xxx) / elapsed)

	psection("gpfs_filesystems");
	for (i = 0; i < records; i++) {
		psub(gpfs_fs_curr[i].fs);
		pstring("node", ip);
		pstring("name", nn);
		plong("rc", gpfs_fs_curr[i].rc); /* status */
		plong("time", gpfs_fs_curr[i].t); /* epoc seconds */
		plong("tu", DELTA_GPFSFS(tu));
		pstring("cl", gpfs_fs_curr[i].cl);
		/*pstring("fs",                gpfs_fs_curr[i].fs); */
		plong("disks", gpfs_fs_curr[i].d);
		plong("readbytes", DELTA_GPFSFS(br));
		plong("writebytes", DELTA_GPFSFS(bw));
		plong("open", DELTA_GPFSFS(oc));
		plong("close", DELTA_GPFSFS(cc));
		plong("reads", DELTA_GPFSFS(rdc));
		plong("writes", DELTA_GPFSFS(wc));
		plong("directorylookup", DELTA_GPFSFS(dir));
		plong("inodeupdate", DELTA_GPFSFS(iu));
		psubend();
	}
	psectionend();

	memcpy((void *) &gpfs_fs_prev[0], (void *) &gpfs_fs_curr[0],
			sizeof(struct gpfs_fs) * records);
}
#endif /* NOGPFS */
/* - - - End of GPFS - - - - */

/*
 read files in format
 name number
 name=number
 name=numer kB
 name=numer bytes
 */
extern long power_timebase; /* lower down this file */
long long purr_prevous = 0;
long long purr_current = 0;
long long pool_idle_time_prevous = 0;
long long pool_idle_time_current = 0;
int lparcfg_found = 0;

void init_lparcfg() {
	FILE *fp = 0;
	char line[1024];
	char label[1024];
	char number[1024];
	int i;
	int len = 0;

	FUNCTION_START;
	if ((fp = fopen("/proc/ppc64/lparcfg", "r")) == NULL) {
		lparcfg_found = 0;
		return;
	} else
		lparcfg_found = 1;

	while (fgets(line, 1000, fp) != NULL) {
		if (!strncmp(line, "purr=", strlen("purr="))) {
			sscanf(line, "%s=%s", label, number);
			purr_current = atoll(number);
		}
		if (!strncmp(line, "pool_idle_time=", strlen("pool_idle_time="))) {
			sscanf(line, "%s=%s", label, number);
			pool_idle_time_current = atoll(number);
		}
	}
}

void read_lparcfg(double elapsed) {
	static FILE *fp = 0;
	static char line[1024];
	char label[1024];
	char number[1024];
	int i;
	int len = 0;

	if (lparcfg_found == 0)
		return;
	FUNCTION_START;
	if (fp == 0) {
		if ((fp = fopen("/proc/ppc64/lparcfg", "r")) == NULL) {
			return;
		}
	} else
		rewind(fp);

	psection("ppc64_lparcfg");
	while (fgets(line, 1000, fp) != NULL) {
		/* lparcfg version strangely with no = */
		if (!strncmp("lparcfg ", line, 8)) {
			line[strlen(line) - 1] = 0; /* remove newline */
			pstring("lparcfg_version", &line[8]);
			continue;
		}
		/* skip the dumb-ass blank line! */
		if (strlen(line) < 2) /* include a single newline */
			continue;

		len = strlen(line);
		/* remove dumb-ass line ending " bytes" */
		if (line[len - 1] == 's' && line[len - 2] == 'e' && line[len - 3] == 't'
				&& line[len - 4] == 'y' && line[len - 5] == 'b'
				&& line[len - 6] == ' ')
			line[len - 6] = 0;

		for (i = 0; i < len; i++) /* strip out the equals sign */
			if (line[i] == '=')
				line[i] = ' ';

		sscanf(line, "%s %s", label, number);
		if (isalpha(number[0]))
			pstring(label, number);
		else {
			plong(label, atoll(number));
			if (!strncmp(line, "purr ", strlen("purr "))) {
				purr_prevous = purr_current;
				purr_current = atoll(number);
				if (purr_prevous != 0 && purr_current != 0)
					pdouble("physical_consumed",
							(double) (purr_current - purr_prevous)
									/ (double) power_timebase / elapsed);
			}
			if (!strncmp(line, "pool_idle_time", strlen("pool_idle_time"))) {
				pool_idle_time_prevous = pool_idle_time_current;
				pool_idle_time_current = atoll(number);
				if (pool_idle_time_prevous != 0 && pool_idle_time_current != 0)
					pdouble("pool_idle_cpu",
							(double) (pool_idle_time_current
									- pool_idle_time_prevous)
									/ (double) power_timebase / elapsed);
			}
		}
	}
	psectionend();
}

#define ADD_LABEL(ch) label[labelch++] = ch
#define ADD_NUM(ch) numstr[numstrch++] = ch
/*
 read files in format
 name number
 name: number
 name: numer kB
 */
void read_data_number(char *statname) {
	FILE *fp = 0;
	char line[1024];
	char filename[1024];
	char label[512];
	char number[512];
	int i;
	int len;

	FUNCTION_START;
	sprintf(filename, "/proc/%s", statname);
	if ((fp = fopen(filename, "r")) == NULL) {
		sprintf(line, "read_data_number: failed to open file %s", filename);
		error(line);
		return;
	}
	sprintf(label, "proc_%s", statname);
	psection(label);
	while (fgets(line, 1000, fp) != NULL) {
		len = strlen(line);
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
		sscanf(line, "%s %s", label, number);
		/*printf("read_data_numer(%s) |%s| |%s|=%lld\n",
		 * statname,label,numstr,atoll(numstr));*/
		plong(label, atoll(number));
	}
	psectionend();
	(void) fclose(fp);
}
/*
 read /proc/stat and unpick
 */
void proc_stat(double elapsed, int print) {
	int len;
	long long user;
	long long nice;
	long long sys;
	long long idle;
	long long iowait;
	long long hardirq;
	long long softirq;
	long long steal;
	long long guest;
	long long guestnice;
	int cpu_total;
	int count;
	int cpuno;
	int i;
	long long value;
	/* Static data */
	static FILE *fp = 0;
	static char line[8192];
	static int max_cpuno;
	/* structure to recall previous values */
	struct utilisation {
		long long user;
		long long nice;
		long long sys;
		long long idle;
		long long iowait;
		long long hardirq;
		long long softirq;
		long long steal;
		long long guest;
		long long guestnice;
	};
#define MAX_LOGICAL_CPU 256
	static long long old_ctxt;
	static long long old_processes;
	static struct utilisation total_cpu;
	static struct utilisation logical_cpu[MAX_LOGICAL_CPU];
	char label[512];

	FUNCTION_START;
	/* printf("DEBUG\t--> proc_stat(%.4f, %d) max_cpuno=%d\n",elapsed,
	 * print,max_cpuno); */
	if (fp == 0) {
		if ((fp = fopen("/proc/stat", "r")) == NULL) {
			error("failed to open file /proc/stat");
			fp = 0;
			return;
		}
	} else
		rewind(fp);

	if (print)
		psection("stat");
	while (fgets(line, 1000, fp) != NULL) {
		len = strlen(line);

		if (!strncmp(line, "cpu", 3)) {
			if (!strncmp(line, "cpu ", 4)) {
				cpu_total = 1;
				count = sscanf(&line[4], /* cpu USER */
				"%lld %lld %lld %lld %lld %lld %lld %lld %lld %lld", &user,
						&nice, &sys, &idle, &iowait, &hardirq, &softirq, &steal,
						&guest, &guestnice);
				if (print) {
#define DELTA_TOTAL(stat) \
  ((float)(stat - total_cpu.stat) / (float)elapsed / ((float)(max_cpuno + 1.0)))
					psub("cpu_total");
					pdouble("user", DELTA_TOTAL(user)); /* incrementing counter */
					pdouble("nice", DELTA_TOTAL(nice)); /* incrementing counter */
					pdouble("sys", DELTA_TOTAL(sys)); /* incrementing counter */
					pdouble("idle", DELTA_TOTAL(idle)); /* incrementing counter */
					/*                        pdouble("DEBUG IDLE idle: %lld %lld %lld\n",
					 * total_cpu.idle, idle, idle-total_cpu.idle); */
					pdouble("iowait", DELTA_TOTAL(iowait)); /* incrementing counter */
					pdouble("hardirq", DELTA_TOTAL(hardirq)); /* incrementing counter */
					pdouble("softirq", DELTA_TOTAL(softirq)); /* incrementing counter */
					pdouble("steal", DELTA_TOTAL(steal)); /* incrementing counter */
					pdouble("guest", DELTA_TOTAL(guest)); /* incrementing counter */
					pdouble("guestnice", DELTA_TOTAL(guestnice)); /* incrementing counter */
					psubend(0);
				}
				total_cpu.user = user;
				total_cpu.nice = nice;
				total_cpu.sys = sys;
				total_cpu.idle = idle;
				total_cpu.iowait = iowait;
				total_cpu.hardirq = hardirq;
				total_cpu.softirq = softirq;
				total_cpu.steal = steal;
				total_cpu.guest = guest;
				total_cpu.guestnice = guestnice;
			} else {
				cpu_total = 0;
				count = sscanf(&line[3], /* cpuNNN USER*/
				"%d %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld", &cpuno,
						&user, &nice, &sys, &idle, &iowait, &hardirq, &softirq,
						&steal, &guest, &guestnice);
				if (cpuno > max_cpuno)
					max_cpuno = cpuno;
				if (cpuno >= MAX_LOGICAL_CPU)
					continue;
				if (print) {
					sprintf(label, "cpu%d", cpuno);
					psub(label);
#define DELTA_LOGICAL(stat) \
  ((float)(stat - logical_cpu[cpuno].stat) / (float)elapsed)
					pdouble("user", DELTA_LOGICAL(user)); /* counter */
					pdouble("nice", DELTA_LOGICAL(nice)); /* counter */
					pdouble("sys", DELTA_LOGICAL(sys)); /* counter */
					pdouble("idle", DELTA_LOGICAL(idle)); /* counter */
					/*                        pdouble("DEBUG IDLE idle: %lld %lld %lld\n",
					 * logical_cpu[cpuno].idle, idle, idle-logical_cpu[cpuno].idle); */
					pdouble("iowait", DELTA_LOGICAL(iowait)); /* counter */
					pdouble("hardirq", DELTA_LOGICAL(hardirq)); /* counter */
					pdouble("softirq", DELTA_LOGICAL(softirq)); /* counter */
					pdouble("steal", DELTA_LOGICAL(steal)); /* counter */
					pdouble("guest", DELTA_LOGICAL(guest)); /* counter */
					pdouble("guestnice", DELTA_LOGICAL(guestnice)); /* counter */
					psubend(0);
				}
				logical_cpu[cpuno].user = user;
				logical_cpu[cpuno].nice = nice;
				logical_cpu[cpuno].sys = sys;
				logical_cpu[cpuno].idle = idle;
				logical_cpu[cpuno].iowait = iowait;
				logical_cpu[cpuno].hardirq = hardirq;
				logical_cpu[cpuno].softirq = softirq;
				logical_cpu[cpuno].steal = steal;
				logical_cpu[cpuno].guest = guest;
				logical_cpu[cpuno].guestnice = guestnice;
			}
		}
		if (!strncmp(line, "ctxt", 4)) {
			value = 0;
			count = sscanf(&line[5], "%lld", &value); /* counter */
			if (count == 1) {
				if (print) {
					psub("counters");
					pdouble("ctxt", ((double) (value - old_ctxt) / elapsed));
				}
				old_ctxt = value;
			}
		}
		if (!strncmp(line, "btime", 5)) {
			value = 0;
			count = sscanf(&line[6], "%lld", &value); /* seconds since boot */
			if (print)
				plong("btime", value);
		}
		if (!strncmp(line, "processes", 9)) {
			value = 0;
			count = sscanf(&line[10], "%lld", &value); /* counter  actually forks */
			if (print)
				pdouble("processes_forks",
						((double) (value - old_processes) / elapsed));
			old_processes = value;
		}
		if (!strncmp(line, "procs_running", 13)) {
			value = 0;
			count = sscanf(&line[14], "%lld", &value);
			if (print)
				plong("procs_running", value);
		}
		if (!strncmp(line, "procs_blocked", 13)) {
			value = 0;
			count = sscanf(&line[14], "%lld", &value);
			if (print) {
				plong("procs_blocked", value);
				psubend(0);
			}
		}
	}
	if (print)
		psectionend();
}

void proc_diskstats(double elapsed, int print) {
	struct diskinfo {
		long dk_major;
		long dk_minor;
		char dk_name[128];
		long long dk_reads;
		long long dk_rmerge;
		long long dk_rkb;
		long long dk_rmsec;
		long long dk_writes;
		long long dk_wmerge;
		long long dk_wkb;
		long long dk_wmsec;
		long long dk_inflight;
		long long dk_time;
		long long dk_backlog;
		long long dk_xfers;
		long long dk_bsize;
	};
	static struct diskinfo current;
	;
	static struct diskinfo *previous;
	;
	static FILE *fp = 0;
	char buf[1024];
	int dk_stats;
	/* popen variables */
	FILE *pop;
	static long disks;
	char tmpstr[1024 + 1];
	long i;
	long j;
	long len;
	char *ptr;

	FUNCTION_START;
	if (fp == (FILE *) 0) {
		/* Just count the number of disks */
		pop = popen("lsblk --nodeps --output NAME,TYPE --raw 2>/dev/null", "r");
		if (pop != NULL) {
			/* throw away the headerline */
			tmpstr[0] = 0;
			ptr = fgets(tmpstr, 127, pop);
			for (disks = 0;; disks++) {
				tmpstr[0] = 0;
				if (fgets(tmpstr, 127, pop) == NULL)
					break;
				/*printf("DEBUG %ld disks - %s\n",disks,tmpstr);*/
			}
			pclose(pop);
		} else
			disks = 0;
		/*printf("DEBUG %ld disks\n",disks); */
		previous = malloc(sizeof(struct diskinfo) * disks);

		pop = popen("lsblk --nodeps --output NAME,TYPE --raw 2>/dev/null", "r");
		if (pop != NULL) {
			/* throw away the headerline */
			ptr = fgets(tmpstr, 70, pop);
			for (i = 0; i < disks; i++) {
				tmpstr[0] = 0;
				if (fgets(tmpstr, 70, pop) == NULL)
					break;
				tmpstr[strlen(tmpstr)] = 0; /* remove NL char */
				len = strlen(tmpstr);
				for (j = 0; j < len; j++)
					if (tmpstr[j] == ' ')
						tmpstr[j] = 0;
				strcpy(previous[i].dk_name, tmpstr);
				/*printf("DEBUG saved %ld %s disk name\n",i,previous[i].dk_name);*/
			}
			pclose(pop);
		} else
			disks = 0;

		if ((fp = fopen("/proc/diskstats", "r")) == NULL) {
			error("failed to open - /proc/diskstats");
			return;
		}
	} else
		rewind(fp);

	if (print)
		psection("disk");
	while (fgets(buf, 1024, fp) != NULL) {
		buf[strlen(buf) - 1] = 0; /* remove newline */
		/*printf("DISKSTATS: \"%s\"", buf);*/
		/* zero the data ready for reading */
		bzero(&current, sizeof(struct diskinfo));
		dk_stats =
				sscanf(&buf[0],
						"%ld %ld %s %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld %lld",
						&current.dk_major, &current.dk_minor,
						&current.dk_name[0], &current.dk_reads,
						&current.dk_rmerge, &current.dk_rkb, &current.dk_rmsec,
						&current.dk_writes, &current.dk_wmerge, &current.dk_wkb,
						&current.dk_wmsec, &current.dk_inflight,
						&current.dk_time, &current.dk_backlog);

		if (dk_stats == 7) { /* shuffle the data around due to missing columns for partitions */

			current.dk_wkb = current.dk_rmsec;
			current.dk_writes = current.dk_rkb;
			current.dk_rkb = current.dk_rmerge;
			current.dk_rmsec = 0;
			current.dk_rmerge = 0;

		} else if (dk_stats != 14)
			fprintf(stderr, "disk sscanf wanted 14 but returned=%d line=%s\n",
					dk_stats, buf);

		current.dk_rkb /= 2; /* sectors = 512 bytes */
		current.dk_wkb /= 2;
		current.dk_xfers = current.dk_reads + current.dk_writes;
		if (current.dk_xfers == 0)
			current.dk_bsize = 0;
		else
			current.dk_bsize = ((current.dk_rkb + current.dk_wkb)
					/ current.dk_xfers) * 1024;

		current.dk_time /= 10.0; /* in milli-seconds to make it upto 100%, 1000/100 = 10 */

		/* loop**** disks are not real */
		/*if(strncmp(current.dk_name,"loop", 4) )
		 break;*/

		for (i = 0; i < disks; i++) {
			/*printf("DEBUG disks new %s old %s\n",
			 * current.dk_name,previous[i].dk_name);*/
			if (!strcmp(current.dk_name, previous[i].dk_name)) {
				if (print) {
					psub(current.dk_name);
					/*
					 printf("major",      current.dk_major);
					 printf("minor",      current.dk_minor);
					 */
					pdouble("reads",
							(current.dk_reads - previous[i].dk_reads)
									/ elapsed);
					/*printf("DEBUG  reads: %lld %lld %.2f,\n",    current.dk_reads,
					 * previous[i].dk_reads,  elapsed); */
					pdouble("rmerge",
							(current.dk_rmerge - previous[i].dk_rmerge)
									/ elapsed);
					pdouble("rkb",
							(current.dk_rkb - previous[i].dk_rkb) / elapsed);
					pdouble("rmsec",
							(current.dk_rmsec - previous[i].dk_rmsec)
									/ elapsed);

					pdouble("writes",
							(current.dk_writes - previous[i].dk_writes)
									/ elapsed);
					pdouble("wmerge",
							(current.dk_wmerge - previous[i].dk_wmerge)
									/ elapsed);
					pdouble("wkb",
							(current.dk_wkb - previous[i].dk_wkb) / elapsed);
					pdouble("wmsec",
							(current.dk_wmsec - previous[i].dk_wmsec)
									/ elapsed);

					plong("inflight", current.dk_inflight);
					pdouble("time",
							(current.dk_time - previous[i].dk_time) / elapsed);
					pdouble("backlog",
							(current.dk_backlog - previous[i].dk_backlog)
									/ elapsed);
					pdouble("xfers",
							(current.dk_xfers - previous[i].dk_xfers)
									/ elapsed);
					plong("bsize", current.dk_bsize);
					psubend(0);
				}
				memcpy(&previous[i], &current, sizeof(struct diskinfo));
				break; /* once found stop searching */
			}
		}
	}
	if (print)
		psectionend();
}

void strip_spaces(char *s) {
	char *p;
	int spaced = 1;

	p = s;
	for (p = s; *p != 0; p++) {
		if (*p == ':')
			*p = ' ';
		if (*p != ' ') {
			*s = *p;
			s++;
			spaced = 0;
		} else if (spaced) {
			/* do no thing as this is second space */
		} else {
			*s = *p;
			s++;
			spaced = 1;
		}
	}
	*s = 0;
}

void proc_net_dev(double elapsed, int print) {
	struct netinfo {
		char if_name[128];
		long long if_ibytes;
		long long if_ipackets;
		long long if_ierrs;
		long long if_idrop;
		long long if_ififo;
		long long if_iframe;
		long long if_obytes;
		long long if_opackets;
		long long if_oerrs;
		long long if_odrop;
		long long if_ofifo;
		long long if_ocolls;
		long long if_ocarrier;
	};
	static struct netinfo current;
	static struct netinfo *previous;
	long long junk;

	static FILE *fp = 0;
	char buf[1024];
	int if_stats;
	static long interfaces;
	int ret;
	/* popen variables */
	FILE *pop;
	char tmpstr[1024 + 1];
	long i;
	long j;
	long len;
	char *ptr;

	FUNCTION_START;
	if (fp == (FILE *) 0) {
		/* Just count the number of UP network interfaces */
		pop = popen("/sbin/ifconfig -s 2>/dev/null", "r");
		if (pop != NULL) {
			/* throw away the headerline */
			tmpstr[0] = 0;
			ptr = fgets(tmpstr, 1024, pop);
			for (interfaces = 0;; interfaces++) {
				tmpstr[0] = 0;
				if (fgets(tmpstr, 1024, pop) == NULL)
					break;
				/*printf("DEBUG %ld intergaces - %s\n",interfaces,tmpstr);*/
			}
			pclose(pop);
		} else
			interfaces = 0;
		/*printf("DEBUG %ld intergaces\n",interfaces); */
		previous = malloc(sizeof(struct netinfo) * interfaces);

		pop = popen("/sbin/ifconfig -s 2>/dev/null", "r");
		if (pop != NULL) {
			/* throw away the headerline */
			ptr = fgets(tmpstr, 1024, pop);
			for (i = 0; i < interfaces; i++) {
				tmpstr[0] = 0;
				if (fgets(tmpstr, 1024, pop) == NULL)
					break;
				tmpstr[strlen(tmpstr)] = 0; /* remove NL char */
				len = strlen(tmpstr);
				for (j = 0; j < len; j++)
					if (tmpstr[j] == ' ')
						tmpstr[j] = 0;
				strcpy(previous[i].if_name, tmpstr);
				/*printf("DEBUG saved %ld %s interfaces
				 * name\n",i,previous[i].if_name);*/
			}
			pclose(pop);
		} else
			interfaces = 0;

		if ((fp = fopen("/proc/net/dev", "r")) == NULL) {
			error("failed to open - /proc/net/dev");
			return;
		}
	} else
		rewind(fp);

	if (fgets(buf, 1024, fp) == NULL)
		return; /* throw away the header line */
	if (fgets(buf, 1024, fp) == NULL)
		return; /* throw away the header line */

	if (print)
		psection("networks");
	while (fgets(buf, 1024, fp) != NULL) {
		strip_spaces(buf);
		bzero(&current, sizeof(struct netinfo));
		ret = sscanf(&buf[0],
				"%s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu "
						"%llu %llu %llu %llu", (char *) current.if_name,
				&current.if_ibytes, &current.if_ipackets, &current.if_ierrs,
				&current.if_idrop, &current.if_ififo, &current.if_iframe, &junk,
				&junk, &current.if_obytes, &current.if_opackets,
				&current.if_oerrs, &current.if_odrop, &current.if_ofifo,
				&current.if_ocolls, &current.if_ocarrier);
		if (ret != 16) {
			fprintf(stderr, "net sscanf wanted 16 returned = %d line=%s\n", ret,
					(char *) buf);
		} else {
			for (i = 0; i < interfaces; i++) {
				/*printf("DEBUG: i=%ld current.if_name=%s, previous=%s
				 * interfaces=%ld\n",i, current.if_name,previous[i].if_name,
				 * interfaces);*/
				if (!strcmp(current.if_name, previous[i].if_name)) {
					if (print) {
						psub(current.if_name);
						pdouble("ibytes",
								(current.if_ibytes - previous[i].if_ibytes)
										/ elapsed);
						pdouble("ipackets",
								(current.if_ipackets - previous[i].if_ipackets)
										/ elapsed);
						pdouble("ierrs",
								(current.if_ierrs - previous[i].if_ierrs)
										/ elapsed);
						pdouble("idrop",
								(current.if_idrop - previous[i].if_idrop)
										/ elapsed);
						pdouble("ififo",
								(current.if_ififo - previous[i].if_ififo)
										/ elapsed);
						pdouble("iframe",
								(current.if_iframe - previous[i].if_iframe)
										/ elapsed);

						pdouble("obytes",
								(current.if_obytes - previous[i].if_obytes)
										/ elapsed);
						pdouble("opackets",
								(current.if_opackets - previous[i].if_opackets)
										/ elapsed);
						pdouble("oerrs",
								(current.if_oerrs - previous[i].if_oerrs)
										/ elapsed);
						pdouble("odrop",
								(current.if_odrop - previous[i].if_odrop)
										/ elapsed);
						pdouble("ofifo",
								(current.if_ofifo - previous[i].if_ofifo)
										/ elapsed);

						pdouble("ocolls",
								(current.if_ocolls - previous[i].if_ocolls)
										/ elapsed);
						pdouble("ocarrier",
								(current.if_ocarrier - previous[i].if_ocarrier)
										/ elapsed);
						psubend(0);
					}
					memcpy(&previous[i], &current, sizeof(struct netinfo));
					break; /* once found stop searching */
				}
			}
		}
	}
	if (print)
		psectionend();
}

void etc_os_release() {
	static FILE *fp = 0;
	char buf[1024 + 1];

	FUNCTION_START;
	if (fp == 0) {
		if ((fp = fopen("/etc/os-release", "r")) == NULL) {
			return;
		}
	} else
		rewind(fp);

	psection("os_release");
	while (fgets(buf, 1024, fp) != NULL) {
		buf[strlen(buf) - 1] = 0; /* remove newline */
		if (buf[strlen(buf) - 1] == '"')
			buf[strlen(buf) - 1] = 0; /* remove double quote */

		if (!strncmp(buf, "NAME=", strlen("NAME="))) {
			pstring("name", &buf[strlen("NAME=") + 1]);
		}
		if (!strncmp(buf, "VERSION=", strlen("VERSION="))) {
			pstring("version", &buf[strlen("VERSION=") + 1]);
		}
		if (!strncmp(buf, "PRETTY_NAME=", strlen("PRETTY_NAME="))) {
			pstring("pretty_name", &buf[strlen("PRETTY_NAME=") + 1]);
		}
		if (!strncmp(buf, "VERSION_ID=", strlen("VERSION_ID="))) {
			pstring("version_id", &buf[strlen("VERSION_ID=") + 1]);
		}
	}
	psectionend();
}

void proc_version() {
	static FILE *fp = 0;
	char buf[1024 + 1];
	int i;

	FUNCTION_START;
	if (fp == 0) {
		if ((fp = fopen("/proc/version", "r")) == NULL) {
			return;
		}
	} else
		rewind(fp);
	if (fgets(buf, 1024, fp) != NULL) {
		buf[strlen(buf) - 1] = 0; /* remove newline */
		for (i = 0; i < strlen(buf); i++) {
			if (buf[i] == '"')
				buf[i] = '|';
		}
		psection("proc_version");
		pstring("version", buf);
		psectionend();
	}
}

void lscpu() {
	FILE *pop = 0;
	int data_col = 21;
	int len = 0;
	char buf[1024 + 1];

	FUNCTION_START;
	if ((pop = popen("/usr/bin/lscpu", "r")) == NULL)
		return;

	buf[0] = 0;
	psection("lscpu");
	while (fgets(buf, 1024, pop) != NULL) {
		buf[strlen(buf) - 1] = 0; /* remove newline */
		/*printf("DEBUG: lscpu line is |%s|\n",buf); */
		if (!strncmp("Architecture:", buf, strlen("Architecture:"))) {
			len = strlen(buf);
			for (data_col = 14; data_col < len; data_col++) {
				if (isalnum(buf[data_col]))
					break;
			}
			pstring("architecture", &buf[data_col]);
		}
		if (!strncmp("Byte Order:", buf, strlen("Byte Order:"))) {
			pstring("byte_order", &buf[data_col]);
		}
		if (!strncmp("CPU(s):", buf, strlen("CPU(s):"))) {
			pstring("cpus", &buf[data_col]);
		}
		if (!strncmp("On-line CPU(s) list:", buf,
				strlen("On-line CPU(s) list:"))) {
			pstring("online_cpu_list", &buf[data_col]);
		}
		if (!strncmp("Off-line CPU(s) list:", buf,
				strlen("Off-line CPU(s) list:"))) {
			pstring("online_cpu_list", &buf[data_col]);
		}
		if (!strncmp("Model:", buf, strlen("Model:"))) {
			pstring("model", &buf[data_col]);
		}
		if (!strncmp("Model name:", buf, strlen("Model name:"))) {
			pstring("model_name", &buf[data_col]);
		}
		if (!strncmp("Thread(s) per core:", buf,
				strlen("Thread(s) per core:"))) {
			pstring("threads_per_core", &buf[data_col]);
		}
		if (!strncmp("Core(s) per socket:", buf,
				strlen("Core(s) per socket:"))) {
			pstring("cores_per_socket", &buf[data_col]);
		}
		if (!strncmp("Socket(s):", buf, strlen("Socket(s):"))) {
			pstring("sockets", &buf[data_col]);
		}
		if (!strncmp("NUMA node(s):", buf, strlen("NUMA node(s):"))) {
			pstring("numa_nodes", &buf[data_col]);
		}
		if (!strncmp("CPU MHz:", buf, strlen("CPU MHz:"))) {
			pstring("cpu_mhz", &buf[data_col]);
		}
		if (!strncmp("CPU max MHz:", buf, strlen("CPU max MHz:"))) {
			pstring("cpu_max_mhz", &buf[data_col]);
		}
		if (!strncmp("CPU min MHz:", buf, strlen("CPU min MHz:"))) {
			pstring("cpu_min_mhz", &buf[data_col]);
		}
		/* Intel only */
		if (!strncmp("BogoMIPS:", buf, strlen("BogoMIPS:"))) {
			pstring("bogomips", &buf[data_col]);
		}
		if (!strncmp("Vendor ID:", buf, strlen("Vendor ID:"))) {
			pstring("vendor_id", &buf[data_col]);
		}
		if (!strncmp("CPU family:", buf, strlen("CPU family:"))) {
			pstring("cpu_family", &buf[data_col]);
		}
		if (!strncmp("Stepping:", buf, strlen("Stepping:"))) {
			pstring("stepping", &buf[data_col]);
		}
	}
	psectionend();
	pclose(pop);
}

void proc_uptime() {
	static FILE *fp = 0;
	char buf[1024 + 1];
	int count;
	long long value;
	long long days;
	;
	long long hours;
	;

	FUNCTION_START;
	if (fp == 0) {
		if ((fp = fopen("/proc/uptime", "r")) == NULL) {
			return;
		}
	} else
		rewind(fp);

	if (fgets(buf, 1024, fp) != NULL) {
		count = sscanf(buf, "%lld", &value);
		if (count == 1) {
			psection("proc_uptime");
			plong("total_seconds", value);
			days = value / 60 / 60 / 24;
			hours = (value - (days * 60 * 60 * 24)) / 60 / 60;
			plong("days", days);
			plong("hours", hours);
			psectionend();
		}
	}
}

void filesystems() {
	FILE *fp;
	struct mntent *fs;
	struct statfs vfs;
	char buf[1024];

	FUNCTION_START;
	if ((fp = setmntent("/etc/mtab", "r")) == NULL)
		error("setmntent(\"/etc/mtab\", \"r\") failed");

	psection("filesystems");
	while ((fs = getmntent(fp)) != NULL) {
		if (fs->mnt_fsname[0] == '/') {
			if (statfs(fs->mnt_dir, &vfs) != 0) {
				sprintf(buf, "%s: statfs failed: %s\n", fs->mnt_dir,
						strerror(errno));
				error(buf);
			}
			/*printf("%s, mounted on %s:\n", fs->mnt_dir, fs->mnt_fsname); */

			psub(fs->mnt_fsname);
			pstring("fs_dir", fs->mnt_dir);
			pstring("fs_type", fs->mnt_type);
			pstring("fs_opts", fs->mnt_opts);

			plong("fs_freqs", fs->mnt_freq);
			plong("fs_passno", fs->mnt_passno);
			plong("fs_bsize", vfs.f_bsize);
			plong("fs_size_mb", (vfs.f_blocks * vfs.f_bsize) / 1024 / 1024);
			plong("fs_free_mb", (vfs.f_bfree * vfs.f_bsize) / 1024 / 1024);
			plong("fs_used_mb",
					(vfs.f_blocks * vfs.f_bsize) / 1024 / 1024
							- (vfs.f_bfree * vfs.f_bsize) / 1024 / 1024);
			pdouble("fs_full_percent",
					((double) vfs.f_blocks - (double) vfs.f_bfree)
							/ (double) vfs.f_blocks * (double) 100.0);
			/*
			 * pdouble("fs_full_percent", ((vfs.f_blocks * vfs.f_bsize) - (vfs.f_bfree
			 * * vfs.f_bsize) ) / (vfs.f_blocks * vfs.f_bsize) * 100.00);
			 */
			plong("fs_avail", (vfs.f_bavail * vfs.f_bsize) / 1024 / 1024);
			plong("fs_files", vfs.f_files);
			plong("fs_files_free", vfs.f_ffree);
			plong("fs_namelength", vfs.f_namelen);
			psubend(0);
		}
	}
	psectionend();
	endmntent(fp);
}

long power_timebase = 0;
long power_nominal_mhz = 0;
int ispower = 0;

void proc_cpuinfo() {
	static FILE *fp = 0;
	char buf[1024 + 1];
	char string[1024 + 1];
	double value;
	int int_val;
	char label[512];
	int processor;

	FUNCTION_START;
	if (fp == 0) {
		if ((fp = fopen("/proc/cpuinfo", "r")) == NULL) {
			return;
		}
	} else
		rewind(fp);

	psection("cpuinfo");
	processor = 0;
	while (fgets(buf, 1024, fp) != NULL) {
		buf[strlen(buf) - 1] = 0; /* remove newline */
		/* moronically cpuinfo file format has Tab characters !!! */
		if (!strncmp("processor", buf, strlen("processor"))) {
			if (processor != 0)
				psubend(0);
			sscanf(&buf[12], "%d", &int_val);
			sprintf(string, "proc%d", processor);
			psub(string);
			processor++;
		}
		if (!strncmp("clock", buf, strlen("clock"))) { /* POWER ONLY */
			sscanf(&buf[9], "%lf", &value);
			pdouble("mhz_clock", value);
			power_nominal_mhz = value; /* save for sys_device_system_cpu() */
			ispower = 1;
		}
		if (!strncmp("vendor_id", buf, strlen("vendor_id"))) {
			pstring("vendor_id", &buf[12]);
		}
		if (!strncmp("cpu MHz", buf, strlen("cpu MHz"))) {
			sscanf(&buf[11], "%lf", &value);
			pdouble("cpu_mhz", value);
		}
		if (!strncmp("cache size", buf, strlen("cache size"))) {
			sscanf(&buf[13], "%lf", &value);
			pdouble("cache_size", value);
		}
		if (!strncmp("physical id", buf, strlen("physical id"))) {
			sscanf(&buf[14], "%d", &int_val);
			plong("physical_id", int_val);
		}
		if (!strncmp("siblings", buf, strlen("siblings"))) {
			sscanf(&buf[11], "%d", &int_val);
			plong("siblings", int_val);
		}
		if (!strncmp("core id", buf, strlen("core id"))) {
			sscanf(&buf[10], "%d", &int_val);
			plong("core_id", int_val);
		}
		if (!strncmp("cpu cores", buf, strlen("cpu cores"))) {
			sscanf(&buf[12], "%d", &int_val);
			plong("cpu_cores", int_val);
		}
		if (!strncmp("model name", buf, strlen("model name"))) {
			pstring("model_name", &buf[13]);
		}
		if (!strncmp("timebase", buf, strlen("timebase"))) { /* POWER only */
			ispower = 1;
			break;
		}
	}
	if (processor != 0)
		psubend(0);
	psectionend();
	if (ispower) {
		psection("cpuinfo_power");
		if (!strncmp("timebase", buf, strlen("timebase"))) { /* POWER only */
			pstring("timebase", &buf[11]);
			power_timebase = atol(&buf[11]);
			plong("power_timebase", power_timebase);
		}
		while (fgets(buf, 1024, fp) != NULL) {
			buf[strlen(buf) - 1] = 0; /* remove newline */
			if (!strncmp("platform", buf, strlen("platform"))) { /* POWER only */
				pstring("platform", &buf[11]);
			}
			if (!strncmp("model", buf, strlen("model"))) {
				pstring("model", &buf[9]);
			}
			if (!strncmp("machine", buf, strlen("machine"))) {
				pstring("machine", &buf[11]);
			}
			if (!strncmp("firmware", buf, strlen("firmware"))) {
				pstring("firmware", &buf[11]);
			}
		}
		psectionend();
	}
}

/* Call this function AFTER proc_cpuinfo as it needs numbers from it */
void sys_device_system_cpu(double elapsed, int print) {
	FILE *fp = 0;
	char filename[1024];
	char line[1024];
	int i;
	int finished = 0;

	double sdelta;
	double pdelta;
	double overclock;
	long long spurr_total;
	long long purr_total;

	static int switch_off = 0;
	static long long purr_saved = 0;
	static long long spurr_saved = 0;

	/* FUNCTION_START; */
	if (switch_off) {
		DEBUG
			printf("DEBUG: switched_off\n");
		return;
	}

	if (lparcfg_found == 0) {
		DEBUG
			printf("DEBUG: lparcfg_found == 0\n");
		return;
	}

	spurr_total = 0;
	purr_total = 0;

	for (i = 0, finished = 0; finished == 0 && i < 192 * 8; i++) {
		sprintf(filename, "/sys/devices/system/cpu/cpu%d/spurr", i);
		if (debug)
			printf("spurr file \"%s\"\n", filename);
		if ((fp = fopen(filename, "r")) == NULL) {
			if (debug)
				printf("spurr opened failed\n");
			if (i == 0) { /* failed on the 1st attempt then no spurr file = never try
			 again */
				switch_off = 1;
			}
			finished = 1;
			break;
		}
		if (fgets(line, 1000, fp) != NULL) {
			line[strlen(line) - 1] = 0;
			if (debug)
				printf("spurr read \"%s\"\n", line);
			spurr_total += strtoll(line, NULL, 16);
		} else {
			if (debug)
				printf("spurr read failed\n");
			finished = 1;
		}
		fclose(fp);

		sprintf(filename, "/sys/devices/system/cpu/cpu%d/purr", i);
		if (debug)
			printf("purr file \"%s\"\n", filename);
		if ((fp = fopen(filename, "r")) == NULL) {
			if (debug)
				printf("purr opened failed\n");
			if (i == 0) { /* failed on the 1st attempt then no purr file = never try
			 again */
				switch_off = 1;
			}
			finished = 1;
			break;
		}
		if (fgets(line, 1000, fp) != NULL) {
			line[strlen(line) - 1] = 0;
			if (debug)
				printf("purr read \"%s\"\n", line);
			purr_total += strtoll(line, NULL, 16);
		} else {
			if (debug)
				printf("purr read failed\n");
			finished = 1;
		}
		fclose(fp);
	}

	if (print == PRINT_FALSE) {
		DEBUG
			printf("DEBUG: PRINT_FALSE\n");
		purr_saved = purr_total;
		spurr_saved = spurr_total;
		return;
	}

	if (purr_total == 0 || spurr_total == 0) {
		switch_off = 1;
		return;
	} else {
		psection("sys_dev_sys_cpu");

		pdelta = (double) (purr_total - purr_saved) / (double) power_timebase
				/ elapsed;
		purr_saved = purr_total;
		pdouble("purr", pdelta);

		sdelta = (double) (spurr_total - spurr_saved) / (double) power_timebase
				/ elapsed;
		spurr_saved = spurr_total;
		pdouble("spurr", sdelta);

		overclock = (double) sdelta / (double) pdelta;
		pdouble("nsp", overclock * 100.0);
		pdouble("nominal_mhz", (double) power_nominal_mhz);
		pdouble("current_mhz", (double) power_nominal_mhz * overclock);
		psectionend();
	}
}

void file_read_one_stat(char *file, char *name) {
	FILE *fp;
	char buf[1024 + 1];

	if ((fp = fopen(file, "r")) != NULL) {
		if (fgets(buf, 1024, fp) != NULL) {
			if (buf[strlen(buf) - 1] == '\n') /* remove last char = newline */
				buf[strlen(buf) - 1] = 0;
			pstring(name, buf);
		}
		fclose(fp);
	}
}

void identity(char *command, char *version) {
	char buf[1024 + 1];
	int i;
	/* hostname */
	char label[512];
	struct addrinfo hints;
	struct addrinfo *info;
	struct addrinfo *p;
	/* user name and id */
	struct passwd *pw;
	uid_t uid;
	/* network IP addresses */
	struct ifaddrs *interfaces = NULL;
	struct ifaddrs *ifaddrs_ptr = NULL;
	char address_buf[INET6_ADDRSTRLEN];
	char *str;

	FUNCTION_START;
	psection("identity");
	get_hostname();
	pstring("hostname", hostname);
	pstring("shorthostname", shorthostname);

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC; /*either IPV4 or IPV6*/
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_CANONNAME;

	if (getaddrinfo(hostname, "http", &hints, &info) == 0) {
		for (p = info, i = 1; p != NULL; p = p->ai_next, i++) {
			sprintf(label, "fullhostname%d", i);
			pstring(label, p->ai_canonname);
		}
	}

	if (getifaddrs(&interfaces) == 0) { /* retrieve the current interfaces */
		for (ifaddrs_ptr = interfaces; ifaddrs_ptr != NULL; ifaddrs_ptr =
				ifaddrs_ptr->ifa_next) {
			if (ifaddrs_ptr->ifa_addr) {
				switch (ifaddrs_ptr->ifa_addr->sa_family) {
				case AF_INET:
					if ((str =
							(char *) inet_ntop(ifaddrs_ptr->ifa_addr->sa_family,
									&((struct sockaddr_in *) ifaddrs_ptr->ifa_addr)->sin_addr,
									address_buf, sizeof(address_buf))) != NULL) {
						sprintf(label, "%s_IP4", ifaddrs_ptr->ifa_name);
						pstring(label, str);
					}
					break;
				case AF_INET6:
					if ((str =
							(char *) inet_ntop(ifaddrs_ptr->ifa_addr->sa_family,
									&((struct sockaddr_in6 *) ifaddrs_ptr->ifa_addr)->sin6_addr,
									address_buf, sizeof(address_buf))) != NULL) {
						sprintf(label, "%s_IP6", ifaddrs_ptr->ifa_name);
						pstring(label, str);
					}
					break;
				default:
					sprintf(label, "%s_Not_Supported_%d", ifaddrs_ptr->ifa_name,
							ifaddrs_ptr->ifa_addr->sa_family);
					pstring(label, "");
					break;
				}
			} else {
				sprintf(label, "%s_network_ignored", ifaddrs_ptr->ifa_name);
				pstring(label, "null_address");
			}
		}

		freeifaddrs(interfaces); /* free the dynamic memory */
	}

	/* POWER and AMD and may be others */
	if (access("/proc/device-tree", R_OK) == 0) {
		file_read_one_stat("/proc/device-tree/compatible", "compatible");
		file_read_one_stat("/proc/device-tree/model", "model");
		file_read_one_stat("/proc/device-tree/part-number", "part-number");
		file_read_one_stat("/proc/device-tree/serial-number", "serial-number");
		file_read_one_stat("/proc/device-tree/system-id", "system-id");
		file_read_one_stat("/proc/device-tree/vendor", "vendor");
	}
	/*x86_64 and AMD64 */
	if (access("/sys/devices/virtual/dmi/id/", R_OK) == 0) {
		file_read_one_stat("/sys/devices/virtual/dmi/id/product_serial",
				"serial-number");
		file_read_one_stat("/sys/devices/virtual/dmi/id/product_name", "model");
		file_read_one_stat("/sys/devices/virtual/dmi/id/sys_vendor", "vendor");
	}

	pstring("njmon_command", command);
	pstring("njmon_version", version);
	uid = geteuid();
	if (pw = getpwuid(uid)) {
		pstring("username", pw->pw_name);
		plong("userid", uid);
	} else {
		pstring("username", "unknown");
	}
	psectionend();
}

/* check_pid_file() and make_pid_file()
 *    If you start njmon and it finds there is a copy running already then it
 * will quitely stop. You can hourly start njmon via crontab and not end up with
 * dozens of copies runnings. It also means if the server reboots then
 * njmon start in the next hour. Side-effect: it creates a file called
 * /tmp/njmon.pid
 *              */
char pid_filename[] = "/tmp/njmon.pid";

void make_pid_file() {
	int fd;
	int ret;
	char buffer[32];

	FUNCTION_START;
	if ((fd = creat(pid_filename, O_CREAT | O_WRONLY)) < 0) {
		printf("can't open new file for writing fd=%d\n", fd);
		perror("open");
		return; /* no file */
	}
	printf("write file descriptor=%d\n", fd);
	sprintf(buffer, "%d \n", getpid());
	printf("write \"%s\"\n", buffer);
	if ((ret = write(fd, buffer, strlen(buffer))) <= 0)
		printf("write failed ret=%d\n", ret);
	close(fd);
}

void check_pid_file() {
	char buffer[32];
	int fd;
	pid_t pid;
	int ret;

	FUNCTION_START;
	if ((fd = open(pid_filename, O_RDONLY)) < 0) {
		printf("no file or can't open it\n");
		make_pid_file();
		return; /* no file */
	}
	printf("file descriptor=%d\n", fd);
	printf("file exists and readable and opened\n");
	if (read(fd, buffer, 31) > 0) { /* has some data */
		printf("file has some content\n");
		buffer[31] = 0;
		if (sscanf(buffer, "%d", &pid) == 1) {
			printf("read a pid from the file OK = %d\n", pid);
			ret = kill(pid, 0);
			printf("kill %d, 0) = returned =%d\n", pid, ret);
			if (ret == 0) {
				printf("we have a njmon running - exit\n");
				exit(13);
			}
		}
	}
	/* if we got here there is a file but the content is duff or the process is
	 * not running */
	close(fd);
	remove(pid_filename);
	make_pid_file();
}

void hint(char *program, char *version) {
	FUNCTION_START;
	printf("%s: help information. Version:%s\n\n", program, version);
	printf("- Performance stats collector outputing JSON format. Default is "
			"stdout\n");
	printf("- Core syntax:     %s -s seconds -c count\n", program);
	printf("- JSON style:      -M  (default) or older style -S or -O\n");
	printf("- File output:     -m directory -f\n");
#ifndef NOREMOTE
	printf("- njmon collector output: -i host -p port -X secret\n");
#endif /* NOREMOTE */
	/* not implemented yet printf("additional options: -P\n"); */
	printf("- Other options: -?\n");
	printf("\n");
	printf("\t-s seconds : seconds between snapshots of data (default 60 "
			"seconds)\n");
	printf("\t-c count   : number of snapshots (default forever)\n\n");
	printf(
			"\t-S         : Single level output format - section names form part of "
					"the value names\n");
	printf("\t-M         : Multiple level output format - section & subsection "
			"names (default)\n");
	printf(
			"\t-O         : Old Multiple level output format - like -M but identity "
					"before samples\n\n");
	printf("\t-m directory : Program will cd to the directory before output\n");
	printf("\t-f         : Output to file (not stdout) to two files below\n");
	printf("\t           : Data:   "
			"hostname_<year><month><day>_<hour><minutes>.json\n");
	printf("\t           : Errors: "
			"hostname_<year><month><day>_<hour><minutes>.err\n");
	printf(
			"\t-k         : Read /tmp/njmon.pid for a running njmon PID & if found "
					"running then this copy exits\n");
	/* not implemented yet printf("\t-P         : Also collect process stats
	 * (these can be large)\n"); */
	printf("\t-?         : This output and stop\n");
	printf("\t-d         : Switch on debugging\n");

#ifndef NOREMOTE
	printf("Push data to collector: add -h hostname -p port\n");
	printf(
			"\t-i ip      : IP address or hostname of the njmon central collector\n");
	printf("\t-p port    : port number on collector host\n");
	printf("\t-X         : Set the remote collector secret or use shell "
			"NJMON_SECRET\n");
#endif /* NOREMOTE */

	printf("\n");
	printf("Examples:\n");
	printf("    1 Every 5 mins all day\n");
	printf("\t/home/nag/njmon -s 300 -c 288 -f -m /home/perf\n");
	printf("    2 Piping to data handler using half a day\n");
	printf("\t/home/nag/njmon -s 30 -c 1440 | myprog\n");
	printf("    3 Use the defaults (-s 60 forever) and save to a file \n");
	printf("\t./njmon > my_server_today.json\n");
	printf("    4 Crontab entry\n");
	printf("\t0 4 * * * /home/nag/njmon -s 300 -c 288 -f -m /home/perf\n");
	printf(
			"    5 Crontab - hourly check/restart remote njmon, pipe stats back & "
					"insert into local DB\n");
	printf(
			"\t* 0 * * * /usr/bin/ssh nigel@server /usr/lbin/njmon -s 300 -c 288 | "
					"/lbin/injector\n");
	printf("    6 Crontab - for pumping data to the njmon central collector\n");
	printf(
			"\t* 0 * * * /usr/local/bin/njmon -s 300 -c 288 -i admin.acme.com -p "
					"8181 -X SECRET42 \n");
	printf("\n");
}

/* MAIN */

int main(int argc, char **argv) {
	char secret[256] = { 'O', 'x', 'd', 'e', 'a', 'd', 'b', 'e', 'e', 'f', 0 };
	long loop;
	long maxloops = -1;
	long seconds = 60;
	long port = -1;
	char host[1024 + 1] = { 0 };
	int hostmode = 0;
	int ch;
	double elapsed;
	double previous_time;
	double current_time;
	struct timeval tv;
	int commlen;
	int i;
	int file_output = 0;
	int directory_set = 0;
	char directory[4096 + 1];
	char filename[4096];
	char *s;
	FILE *fp;
	int print_child_pid = 0;
	char datastring[256];
	pid_t childpid;
	int *crashptr = NULL;

	FUNCTION_START;
	s = getenv("NJMON_SECRET");
	if (s != 0)
		debug = atoi(s);
	s = getenv("NJMON_STATS");
	if (s != 0)
		njmon_stats = atoi(s);
	s = getenv("NJMON_SECRET");
	if (s != 0)
		strncpy(secret, s, 128);

	signal(SIGUSR1, interrupt);
	signal(SIGUSR2, interrupt);

	while (-1 != (ch = getopt(argc, argv, "?hfm:SMOs:c:kdi:p:X:x"))) {
		switch (ch) {
		case '?':
		case 'h':
			hint(argv[0], VERSION);
			exit(0);
		case 'f':
			file_output = 1;
			break;
		case 'm':
			directory_set = 1;
			strncpy(directory, optarg, 4096);
			directory[4096] = 0;
			break;
		case 'S':
			mode = ONE_LEVEL;
			break;
		case 'M':
			mode = MULTI_LEVEL;
			break;
		case 'O':
			mode = MULTI_LEVEL;
			oldmode = 1;
			break;

		case 's':
			seconds = atoi(optarg);
			if (seconds < 1)
				seconds = 1;
			break;
		case 'c':
			maxloops = atoi(optarg);
			break;
		case 'k':
			check_pid_file();
			break;
		case 'd':
			debug++;
			break;
#ifndef NOREMOTE
		case 'i':
			strncpy(host, optarg, 1024);
			host[1024] = 0;
			hostmode = 1;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'X':
			strncpy(secret, optarg, 128);
			break;
#endif /* NOREMOTE */
		case 'x':
			print_child_pid = 1;
			break;
		}
	}
#ifndef NOREMOTE
	if (hostmode == 1 && port <= 0) {
		printf("%s -i %s set but not the -p port option\n", argv[0], host);
		exit(52);
	}
	if (hostmode == 0 && port > 0) {
		printf("%s -p %ld but not the -i ip-address option\n", argv[0], port);
		exit(53);
	}
	if (hostmode == 1 && port != 0) { /* We are attempting sending the data remotely */
		if (isalpha(host[0])) {
			struct hostent *he;

			he = gethostbyname(host);
			if (he == NULL) {
				printf(
						"hostname=%s to IP address convertion failed, bailing out\n",
						hostname);
				exit(98);
			}
			/*
			 printf("name=%s\n",he->h_name);
			 printf("type=%d = ",he->h_addrtype);
			 switch(he->h_addrtype) {
			 case AF_INET: printf("IPv4\n"); break;
			 case AF_INET6: printf("(IPv6\n"); break;
			 default: printf("unknown\n");
			 }
			 printf("length=%d\n",he->h_length);
			 */

			/* this could return multiple IP addresses but we assume its the first one
			 */
			if (he->h_addr_list[0] != NULL) {
				strcpy(host,
						inet_ntoa(*(struct in_addr *) (he->h_addr_list[0])));
			} else {
				printf(
						"hostname=%s to IP address convertion failed, bailing out\n",
						host);
				exit(99);
			}
		}

		get_hostname();
		get_time();
		get_utc();
		sprintf(datastring, "%04d-%02d-%02dT%02d:%02d:%02d", tim->tm_year,
				tim->tm_mon, tim->tm_mday, tim->tm_hour, tim->tm_min,
				tim->tm_sec);
		create_socket(host, port, hostname, datastring, secret);
	}
#endif /* NOREMOTE */

	if (directory_set) {
		if (chdir(directory) == -1) {
			perror("Change Directory failed");
			printf("Directory attempted was: %s\n", directory);
			exit(11);
		}
	}

	if (file_output) {
		get_hostname();
		get_time();
		get_localtime();
		sprintf(filename, "%s_%02d%02d%02d_%02d%02d.json", shorthostname,
				tim->tm_year, tim->tm_mon, tim->tm_mday, tim->tm_hour,
				tim->tm_min);
		if ((fp = freopen(filename, "w", stdout)) == 0) {
			perror("opening file for stdout");
			fprintf(stderr, "ERROR nmon filename=%s\n", filename);
			exit(13);
		}
		sprintf(filename, "%s_%02d%02d%02d_%02d%02d.err", hostname,
				tim->tm_year, tim->tm_mon, tim->tm_mday, tim->tm_hour,
				tim->tm_min);
		if ((fp = freopen(filename, "w", stderr)) == 0) {
			perror("opening file for stderr");
			fprintf(stderr, "ERROR nmon filename=%s\n", filename);
			exit(14);
		}
	}
	fflush(NULL);
	/* disconnect from terminal */
	DEBUG
		printf("forking for daemon making if debug=%d === 0\n", debug);
	if (!debug && (childpid = fork()) != 0) {
		if (print_child_pid)
			printf("%d\n", childpid);
		exit(0); /* parent returns OK */
	}
	DEBUG
		printf("child running\n");
	if (!debug) {
		/*        close(0);
		 close(1);
		 close(2);
		 */
		setpgrp(); /* become process group leader */
		signal(SIGHUP, SIG_IGN); /* ignore hangups */
	}
	output_size = 1024 * 1024;
	output = malloc(output_size); /* buffer space for the stats before the push to
	 standard output */
	commlen = 1; /* for the terminating zero */
	for (i = 0; i < argc; i++) {
		commlen = commlen + strlen(argv[i]) + 1; /* +1 for spaces */
	}
	command = malloc(commlen);
	command[0] = 0;
	for (i = 0; i < argc; i++) {
		strcat(command, argv[i]);
		if (i != (argc - 1))
			strcat(command, " ");
	}

	/* seed incrementing counters */
	proc_stat(elapsed, PRINT_FALSE);
	proc_diskstats(elapsed, PRINT_FALSE);
	proc_net_dev(elapsed, PRINT_FALSE);
	init_lparcfg();
	sys_device_system_cpu(1.0, PRINT_FALSE);
#ifndef NOGPFS
	gpfs_init();
#endif /* NOGPFS */

	gettimeofday(&tv, 0);
	current_time = (double) tv.tv_sec + (double) tv.tv_usec * 1.0e-6;
	/* first time just sleep(1) so the first snapshot has some real-ish data */
	if (seconds <= 60)
		sleep(seconds);
	else
		sleep(60); /* if a long time between snapshot do a quick one now so we have
		 one in the bank */

	/* pre-amble */
	if (mode == ONE_LEVEL) {
		praw("[\n");
	}
	if (mode == MULTI_LEVEL) {
		pstart();
		if (oldmode)
			identity(argv[0], VERSION);
		praw("  \"samples\": [\n");
	}
	for (loop = 0; maxloops == -1 || loop < maxloops; loop++) {
		psample();
		if (loop != 0)
			sleep(seconds);
		/* calculate elapsed time to include sleep and data collection time */
		previous_time = current_time;
		gettimeofday(&tv, 0);
		current_time = (double) tv.tv_sec + ((double) tv.tv_usec * 1.0e-6);
		elapsed = current_time - previous_time;

		if (mode == ONE_LEVEL) {
			identity(argv[0], VERSION);
		}

		date_time(seconds, loop, maxloops);
		if (!oldmode)
			identity(argv[0], VERSION);
		etc_os_release();
		proc_version();
		lscpu();
		proc_stat(elapsed, PRINT_TRUE);
		proc_cpuinfo();
		read_data_number("meminfo");
		read_data_number("vmstat");
		proc_diskstats(elapsed, PRINT_TRUE);
		proc_net_dev(elapsed, PRINT_TRUE);
		proc_uptime();
		filesystems();
		read_lparcfg(elapsed);
		sys_device_system_cpu(elapsed, PRINT_TRUE);
#ifndef NOGPFS
		gpfs_data(elapsed);
#endif /* NOGPFS */

		DEBUG
			praw("Sample");
		psampleend(loop == (maxloops - 1));
		push();
		/* debbuging - uncomment to crash here!
		 *crashptr = 42;
		 *crashptr = 42;
		 */
	}
	/* finish-of */
	if (mode == ONE_LEVEL) {
		remove_ending_comma_if_any();
		praw("]\n");
		if (njmon_stats)
			pstats();
	}
	if (mode == MULTI_LEVEL) {
		remove_ending_comma_if_any();
		praw(" ]\n");
		if (njmon_stats)
			pstats();
		pfinish();
	}
	push();
	return 0;
}
/* - - - the end - - - */
