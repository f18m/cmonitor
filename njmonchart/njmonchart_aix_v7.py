#!/usr/bin/python3 
import sys
cmd=    sys.argv[0]
infile= sys.argv[1]
try:
    outfile=sys.argv[2]
except:
    outfile=infile[:-4] + "html"
# read the raw .json as text
f = open(infile,"r")
text = f.read()
f.close()
# fix up the end of the file if it is not complete
ending=text[-3:-1]  #The last two character
if ending == "},":   # the data capture is still running or halted
    text= text[:-2] + "\n ]\n}\n"
# Convert the text to json and extract the stats
import json
entry = []
entry = json.loads(text) # convert text to JSON
jdata = entry["samples"] # removes outer parts so we have a list of snapshot dictionaries

# - - - - - Start nchart functions
chartnum = 10
next_graph_need_stacking = 0

def nchart_start(file, title):
        ''' Head of the HTML webpage'''
        file.write('<html>' + '\n')
        file.write('<head>' + '\n')
        file.write('\t<title>' + title + '</title>\n')
        file.write('\t<script type="text/javascript" src="https://www.google.com/jsapi"></script>\n')
        file.write('\t<script type="text/javascript">\n')
        file.write('\tgoogle.load("visualization", "1.1", {packages:["corechart"]});\n')
        file.write('\tgoogle.setOnLoadCallback(setupCharts);\n')
        file.write('\tfunction setupCharts() {\n')
        file.write('\tvar chart = null;\n')
    
def nchart_bubble_top(file, columnnames):
        ''' Before the graph data with datetime + multiple columns of data '''
        file.write('\tvar data_' + str(chartnum) +  ' = google.visualization.arrayToDataTable([\n')
        file.write("[" + columnnames  + "]\n")
    
def nchart_line_top(file, columnnames):
        ''' Before the graph data with datetime + multiple columns of data '''
        file.write('\tvar data_' + str(chartnum) +  ' = google.visualization.arrayToDataTable([\n')
        file.write("[{type: 'datetime', label: 'Datetime'}," + columnnames  + "]\n")
    
def nchart_bubble_bot(file, graphtitle):
        ''' After the JavaSctipt bubble graph data is output, the data is terminated and the bubble graph options set'''
        global chartnum
        file.write('\t]);\n')
        file.write('\tvar options_'+ str(chartnum) + ' = {\n')
        file.write('\t\tchartArea: {left: "5%", width: "85%", top: "10%", height: "80%"},\n')
        file.write('\t\ttitle: "' + graphtitle + '",\n')
        file.write('\t\thAxis: { title:"CPU seconds in Total"},\n')
        file.write('\t\tvAxis: { title:"Character I/O in Total"},\n')
        file.write('\t\tsizeAxis: {maxSize: 200},\n')
        file.write('\t\tbubble: {textStyle: {fontSize: 15}}\n')
        file.write('\t};\n')
        file.write('\tdocument.getElementById("draw_'+ str(chartnum) + '").addEventListener("click", function() {\n')
        file.write('\tif (chart && chart.clearChart) chart.clearChart();\n')
        file.write('\tchart = new google.visualization.BubbleChart(document.getElementById("chart_master"));\n')
        file.write('\tchart.draw( data_'+ str(chartnum) + ', options_'+ str(chartnum) + ');\n')
        file.write('\t});\n')
        chartnum += 1
    
def nchart_line_bot(file, graphtitle):
        ''' After the JavaSctipt line graph data is output, the data is terminated and the graph options set'''
        global next_graph_need_stacking
        global chartnum 
        file.write('\t]);\n')
        file.write('\tvar options_'+ str(chartnum) + ' = {\n')
        file.write('\t\tchartArea: {left: "5%", width: "85%", top: "10%", height: "80%"},\n')
        file.write('\t\ttitle: "' + graphtitle + '",\n')
        file.write('\t\tfocusTarget: "category",\n')
        file.write('\t\thAxis: { gridlines: { color: "lightgrey", count: 30 } },\n')
        file.write('\t\tvAxis: { gridlines: { color: "lightgrey", count: 11 } },\n')
        file.write('\t\texplorer: { actions: ["dragToZoom", "rightClickToReset"],\n')
        file.write('\t\taxis: "horizontal", keepInBounds: true, maxZoomIn: 20.0 },\n')
        if next_graph_need_stacking:
            file.write('\t\tisStacked:  1\n')
            next_graph_need_stacking = 0
        else:
            file.write('\t\tisStacked:  0\n')
        file.write('\t};\n')
        file.write('\tdocument.getElementById("draw_'+ str(chartnum) + '").addEventListener("click", function() {\n')
        file.write('\tif (chart && chart.clearChart) chart.clearChart();\n')
        file.write('\tchart = new google.visualization.AreaChart(document.getElementById("chart_master"));\n')
        file.write('\tchart.draw( data_'+ str(chartnum) + ', options_'+ str(chartnum) + ');\n')
        file.write('\t});\n')
        chartnum += 1
    
def nchart_end(file, name, config, buttons, summary):
        ''' Generic version using named arguments for 1 to 10 buttons for Server graphs - Finish off the web page '''
        file.write('\t}\n')
        file.write(config)
        file.write('\t</script>\n')
        file.write('\t</head>\n')
        file.write('\t<body bgcolor="#EEEEFF">\n')
        file.write('\t<b>Server: ' + name + ': </b>\n')
        file.write('<button onclick="config()"><b>Configuration</b></button>\n')
        # - - - loop through the buttons and change colours
        colour='black' 
        for num,name in enumerate(buttons, start=10):
           if(name == 'CPU Core'):
               colour='red'
           if(name == 'RAM Use'):
               colour='blue'
           if(name == 'SysCalls'):
               colour='orange'
           if(name == 'TotalDisk-MB'):
               colour='brown'
           if(name == 'TotalNet-Bytes'):
               colour='purple'
           if(name == 'IPC'):
               colour='green'
           file.write('\t<button id="draw_' + str(num) + '" style="color:' + colour + '"><b>'+ name + '</b></button>\n')
        file.write('\t<div id="chart_master" style="width:100%; height:85%;">\n')
        file.write('\t<h2 style="color:blue">Click on a Graph button above, to display that graph</h2>\n')
        file.write('\t</div><br>\n')
        file.write('<table><tr><td>')
        for i,entry in enumerate(summary, start=1):
            file.write("<li>" + entry + "\n")
            if((i % 4) == 0): file.write("<td>\n")
        file.write('</table>\n')
        file.write('Author: Nigel Griffiths @mr_nmon generated by njmon + njmonchart v7    To Zoom = Left-click and drag. To Reset = Right-click.\n')
        file.write('</body>\n')
        file.write('</html>\n')

# convert ISO date like 2017-08-21T20:12:30 to google date+time 2017,04,21,20,12,30
def googledate(date):
        d = date[0:4] + "," +  str(int(date[5:7]) -1) + "," + date[8:10] + "," + date[11:13] + "," + date[14:16] + "," + date[17:19]
        return d
# - - - - - The nchart function End

# These are flags used as function arguments
stacked=1
unstacked=0

def graphit(web,column_names,data,title,button,stack_state):
    global next_graph_need_stacking
    nchart_line_top(web, column_names)
    web.write(data)    
    next_graph_need_stacking = stack_state
    nchart_line_bot(web, title)
    buttonlist.append(button)
 
def bubbleit(web,column_names,data,title,button):
    nchart_bubble_top(web, column_names)
    web.write(data)    
    nchart_bubble_bot(web, title)
    buttonlist.append(button)
 
# ----- MAIN SCRIPT PREPARE DATA -

# initialise some useful content
buttonlist = []
details = ' for LPAR=' + jdata[0]['identity']['hostname']
details = details + ' Server=' + jdata[0]['server']['machine_type']
details = details + ' Serial=' + jdata[0]['server']['serial_no']
details = details + ' OS=AIX %.1f.%d.%d Year=%d Week=%d'%(jdata[0]['server']['aix_version'],
                                                          jdata[0]['server']['aix_technology_level'],
                                                          jdata[0]['server']['aix_service_pack'],
                                                          jdata[0]['server']['aix_build_year'],
                                                          jdata[0]['server']['aix_build_week'])
hostname = jdata[0]['identity']['hostname'] 
cpu_data=""
mhz_data=""
pcpu_data=""
lcpu_data=""
rq_data=""
sc_data=""
ps_data=""
rw_data = ""
rwmb_data = ""
fe_data = ""
ds_data = ""
la_data = ""
mem_data = ""
zf_data = ""
pg_data = ""
pa_data = ""
db_data = ""
dtrw_data = ""
dtt_data = ""
nio_data = ""
np_data = ""
ipc_data = ""
ni_data = ""
di_data = ""
oc_data = ""
pc_data = ""

for i,s in enumerate(jdata):
    if( i == 0 ):
        continue
    cpu_data += ",['Date(%s)', %d,%.1f,%.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']), 
                s['lpar_format2']['online_vcpus'], 
                s['cpu_util']['entitlement'], 
                s['cpu_util']['physical_busy'], 
                s['cpu_util']['physical_consumed'])

    mhz_data += ",['Date(%s)', %.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']), 
            s['cpu_util']['nominal_mhz'], 
            s['cpu_util']['current_mhz'] )

    oc_data += ",['Date(%s)', %.1f]\n" %(googledate(s['timestamp']['datetime']), s['cpu_util']['freq_pct'])

    pc_data += ",['Date(%s)', %.1f, %.1f, %.1f, %.1f, %.1f, %.1f]\n" %(googledate(s['timestamp']['datetime']),
            s['lpar_format2']['entitled_capacity'], 
            s['lpar_format2']['max_pool_capacity'], 
            s['lpar_format2']['entitled_pool_capacity'], 
            s['lpar_format2']['pool_busy_time'] / 100.0, 
            s['lpar_format2']['pool_scaled_busy_time'] / 100.0, 
            s['cpu_util']['physical_consumed'])

    pcpu_data += ",['Date(%s)', %d, %d, %d, %d]\n" %(googledate(s['timestamp']['datetime']),
            s['total_physical_cpu']['user'], 
            s['total_physical_cpu']['sys'], 
            s['total_physical_cpu']['wait'], 
            s['total_physical_cpu']['idle'])

    lcpu_data += ",['Date(%s)', %d, %d, %d, %d]\n" %(googledate(s['timestamp']['datetime']),
            s['total_logical_cpu']['user'], 
            s['total_logical_cpu']['sys'], 
            s['total_logical_cpu']['wait'], 
            s['total_logical_cpu']['idle'])

    try:
        rq = s['kernel']['run_queue']
    except:
        rq = s['kernel']['runqueue']
    rq_data += ",['Date(%s)', %.1f]\n" %(googledate(s['timestamp']['datetime']),rq)

    sc_data += ",['Date(%s)', %d]\n" %(googledate(s['timestamp']['datetime']), s['kernel']['syscall'])

    ps_data += ",['Date(%s)', %d]\n" %(googledate(s['timestamp']['datetime']), s['kernel']['pswitch'])

    rw_data += ",['Date(%s)', %d,%d]\n" %(googledate(s['timestamp']['datetime']),
            s['kernel']['sysread'], s['kernel']['syswrite'])

    di_data += ",['Date(%s)', %d]\n" %(googledate(s['timestamp']['datetime']), s['kernel']['decrintrs'])

    rwmb_data += ",['Date(%s)', %.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']),
                s['kernel']['readch']/1024/1024, s['kernel']['writech']/1024/1024)

    fe_data += ",['Date(%s)', %.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']),
                s['kernel']['sysexec'], s['kernel']['sysexec'])

    ds_data += ",['Date(%s)', %.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']),
                s['kernel']['devintrs'], s['kernel']['softintrs'])

    la_data += ",['Date(%s)', %d, %d, %d]\n" %(googledate(s['timestamp']['datetime']),
                s['kernel']['load_avg_1_min'],
                s['kernel']['load_avg_5_min'],
                s['kernel']['load_avg_15_min'])

    mem_data += ",['Date(%s)', %.1f,%.1f, %.1f,%.1f, %.1f,%.1f, %.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']),
                s['memory']['real_total']/1024,
                s['memory']['real_free']/1024, 
                s['memory']['real_pinned']/1024,
                s['memory']['real_inuse']/1024, 
                s['memory']['real_system']/1024,
                s['memory']['real_user']/1024, 
                s['memory']['real_process']/1024,
                s['memory']['real_avail']/1024 )

    zf_data += ",['Date(%s)', %d]\n" %(googledate(s['timestamp']['datetime']), s['vminfo']['zerofills'])

    pg_data += ",['Date(%s)', %d, %d, %d]\n" %(googledate(s['timestamp']['datetime']),
                s['memory']['pgsp_total'],
                s['memory']['pgsp_free'],
                s['memory']['pgsp_rsvd'])

    pa_data += ",['Date(%s)', %.1f,%.1f, %.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']),
                s['memory']['pgins'],
                s['memory']['pgouts'],
                s['memory']['pgspins'],
                s['memory']['pgspouts'])

    ipc_data += ",['Date(%s)', %.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']),
                s['kernel']['sema'], s['kernel']['msg'])

    ni_data += ",['Date(%s)', %.1f,%.1f,%.1f]\n" %(googledate(s['timestamp']['datetime']),
                s['kernel']['namei'],
                s['kernel']['iget'],
                s['kernel']['dirblk'])

# - - - Disks
    read_mbps=0.0
    write_mbps=0.0
    for disk in s["disks"].keys():
            read_mbps  = read_mbps  + s["disks"][disk]["read_mbps"]
            write_mbps = write_mbps + s["disks"][disk]["write_mbps"]
    dtrw_data += ",['Date(%s)', %.1f, %.1f]\n" %(googledate(s['timestamp']['datetime']), read_mbps, -write_mbps)

    xfers=0
    for disk in s["disks"].keys():
        xfers += s["disks"][disk]["xfers"]
    dtt_data += ",['Date(%s)', %.1f]\n" %(googledate(s['timestamp']['datetime']), xfers)

# - - - Networks
    ibytes=0
    obytes=0
    for net in s["network_interfaces"].keys():
        ibytes   = ibytes   + s["network_interfaces"][net]["ibytes"]
        obytes   = obytes   + s["network_interfaces"][net]["obytes"]
    nio_data += ",['Date(%s)', %.1f, %.1f]\n" %(googledate(s['timestamp']['datetime']), ibytes, -obytes)

    ipackets=0.0
    opackets=0.0
    for net in s["network_interfaces"].keys():
        ipackets = ipackets + s["network_interfaces"][net]["ipackets"]
        opackets = opackets + s["network_interfaces"][net]["opackets"]
    np_data += ",['Date(%s)', %.1f, %.1f]\n" %(googledate(s['timestamp']['datetime']), ipackets,-opackets)

# - - - Top 20 Processes
# Check if there are process stats in this njmon .json file as they are optional
start_processes = "unknown"
end_processes   = "unknown"
try:
    start_processes = len(jdata[0]["processes"])
    end_processes   = len(jdata[-1]["processes"])
    process_data_found = True
except:
    process_data_found = False

if (process_data_found):
    top={}   # start with empty dictionary
    for sam in jdata:
        for process in sam["processes"]:
            entry=sam['processes'][process]
            if (entry['ucpu_time'] != 0.0 and entry['scpu_time'] != 0.0):
                #print("%20s pid=%d ucpu=%0.3f scpu=%0.3f mem=%0.3f io=%0.3f"%(entry['name'],entry['pid'],
                #        entry['ucpu_time'],entry['scpu_time'],
                #        entry['real_mem_data']+entry['real_mem_text'],
                #        entry['inBytes']+entry['outBytes']))
                try:    # update the current entry
                    top[entry['name']]["cpu"] += entry['ucpu_time']+entry['scpu_time']
                    top[entry['name']]["io"]  += entry['inBytes']+entry['inBytes']
                    top[entry['name']]["mem"] += entry['real_mem_data']+entry['real_mem_text']
                except: # no current entry so add one
                    top.update( {entry['name']: { "cpu": entry['ucpu_time']+entry['scpu_time'],
                              "io": entry['inBytes']+entry['outBytes'],
                              "mem": entry['real_mem_data']+entry['real_mem_text']} } )
    def sort_key(d):
        return top[d]['cpu']

    topprocs = ""
    tops = []
    for i,proc in enumerate(sorted(top, key=sort_key, reverse=True)):
        p=top[proc]
        #print("%20s cpu=%0.3f io=%0.3f mem=%0.3f"%(proc, p['cpu'],p['io'],p['mem']))
        topprocs +=  ",['%s',%.1f,%.1f,'%s',%.1f]\n" %(proc, p['cpu'], p['io'], proc, p['mem'])
        tops.append(proc)
        if(i >= 20 ): # Only graph the top 20
            break

    topprocs_title = "'Command', 'CPU seconds', 'CharIO', 'Type', 'Memory KB'"

    top_header = ""
    for proc in tops:
        top_header += "'" + proc + "',"
    top_header = top_header[:-1]

    top_data = ""
    for sam in jdata:
        top_data += ",['Date(%s)'" %(googledate(sam['timestamp']['datetime']))
        for item in tops:
            bytes = 0
            for proc in sam['processes']:
                p = sam['processes'][proc]
                if(p['name'] == item):
                    bytes += p['ucpu_time'] + p['scpu_time']
            top_data += ", %.1f" %(bytes)
        top_data += "]\n"
    # print(top_header)
    # print(top_data)

# - - - Top 20 Disks
tdisk={}   # start with empty dictionary
for sam in jdata:
        for disk in sam["disks"]:
            entry=sam['disks'][disk]
            bytes=entry['read_mbps']+entry['write_mbps']
            if (bytes != 0):
                #print("disk=%s total bytes=%.1f"%(disk,bytes))
                try:    # update the current entry
                    tdisk[entry[disk]] += bytes
                except: # no current entry so add one
                    tdisk.update( {disk: bytes} )
def sort_dkey(d):
        return tdisk[d]

topdisks = []
for i,disk in enumerate(sorted(tdisk, key=sort_dkey, reverse=True)):
    d=tdisk[disk]
    #print("disk=%s total bytes=%.1f"%(disk,bytes))
    topdisks.append(disk)
    if(i >= 20 ): # Only graph the top 20
        break
#print(topdisks)

td_header = ""
for disk in topdisks:
    td_header += "'" + disk + "',"
td_header = td_header[:-1]

td_data = ""
for sam in jdata:
    td_data += ",['Date(%s)'" %(googledate(sam['timestamp']['datetime']))
    for item in topdisks:
        bytes = sam['disks'][item]['read_mbps'] + sam['disks'][item]['write_mbps']
        td_data += ", %.1f" %(bytes)
    td_data += "]\n"
# print(td_header)
# print(td_data)

# ----- add config box 
def configdump(section,string):
    newstr = ''
    thing=jdata[0][section]
    for label in thing:
        newstr = newstr + "%20s = %s<br>\\\n"%(label,str(thing[label]))
    return string + newstr

config_str = '\nfunction config() {\n' + '    var myWindow = window.open("", "MsgWindow", "width=1024, height=800");\n' + \
          '    myWindow.document.write("<h2>Configuration data <br>Use PageDown or Scroll bar (if available)</h2><br>\\\n'
config_str = configdump("identity",config_str)
config_str = configdump("timestamp",config_str)
config_str = configdump("config",config_str)
config_str = configdump("server",config_str)
config_str = configdump("lpar_format2",config_str)
config_str = config_str + '");\n}\n\n'

# ----- MAIN SCRIPT CREAT WEB FILE -
web = open(outfile,"w")        # Open the output file
nchart_start(web, 'Hostname:%s' + hostname)

# ----- add graphs 
if( process_data_found ):
    bubbleit(web, topprocs_title, topprocs,  'Top Processes Summary' + details, "TopSum")
    graphit(web, top_header, top_data,  'Top Procs by CPU time' + details, "TopProcs",unstacked)

graphit(web, td_header, td_data,  'Top Disks (mbps)' + details, "TopDisks",unstacked)
graphit(web, "'VP', 'Entitled', 'Busy', 'Consumed'", cpu_data,  'CPU cores' + details, "CPU Core",unstacked)
graphit(web, "'MHz Nominal', 'MHz Current'", mhz_data,  'CPU MHz' + details, "CPU MHz",unstacked)
graphit(web, "'Overclock'", oc_data,  'Frequency percent of Nominal MHz' + details, "Overclock",unstacked)
graphit(web, "'Entitled','Pool Size','Assigned to LPARs','Busy', 'Scaled Busy', 'Consumed'", pc_data,  'CPU Pool' + details, "CPU Pool",unstacked)
graphit(web, "'User','System','Wait','Idle'", pcpu_data,  'Total Physical CPU (stacked)' + details, "Pcpu",stacked)
graphit(web, "'User','System','Wait','Idle'", lcpu_data,  'Total Logical CPU (stacked)' + details, "Lcpu",stacked)
graphit(web, "'Run Queue'", rq_data,  'CPU Run Queue' + details, "RunQueue",unstacked)
graphit(web, "'1_min_LoadAvg', '5_min_LoadAvg','15_min_LoadAvg'", la_data,  'Load Average' + details, "LoadAvg",unstacked)
graphit(web, "'System Calls'", sc_data,  'System Calls' + details, "SysCalls",unstacked)
graphit(web, "'Process Switch'", ps_data,  'Process Switches' + details, "pSwitch",unstacked)
graphit(web, "'Reads', 'Writes'", rw_data,  'Systems Calls Read & Write' + details, "SysCall-RW",unstacked)
graphit(web, "'Reads-MB', 'Writes-MB'", rw_data,  'Systems Calls Read MB & Write MB' + details, "SysCall-RWMB",unstacked)
graphit(web, "'Fork', 'Exec'", fe_data,  'Systems Calls fork() & exec()' + details, "Fork-Exec",unstacked)
graphit(web, "'Device', 'Soft'", ds_data,  'Interrupts by HW Device & Soft' + details, "Interrupts",unstacked)
graphit(web, "'Decr-Interrupts'", di_data,  'Decrementor Interrupts' + details, "DecIntr",unstacked)
graphit(web, "'Memory Size', 'Memory Free', 'Pinned', 'inuse', 'system', 'user', 'process', 'avail'", mem_data, 'Memory MB' + details, "Memory",unstacked)
graphit(web, "'zero fills'", zf_data,  'Memory Page Zero Fill' + details, "MemoryZero",unstacked)
graphit(web, "'Size', 'Free', 'Reserved'", pg_data,  'Paging Space in MB' + details, "PageSpace",unstacked)
graphit(web, "'PgIn', 'PgOut', 'PgSpaceIn','PgSpaceOut'", pa_data,  'Paging Filesystem(in & out) + Paging Space(in & out)' + details, "Paging",unstacked)
graphit(web, "'Read', 'Write'", dtrw_data,  'Disk Total Read & Write MB/s' + details, "TotalDisk-MB",unstacked)
graphit(web, "'Disk Transfer'", dtt_data,  'Disk total transfers/s' + details, "TotalDisk-xfer",unstacked)

# - - - Disks
dstr = ""
for device in jdata[0]["disks"].keys():
    dstr = dstr + "'" + device + "',"
dstr = dstr[:-1]
nchart_line_top(web, dstr)
for i,s in enumerate(jdata):
    if i == 0:
        continue
    web.write(",['Date(%s)' " %(googledate(s['timestamp']['datetime'])))
    for device in s["disks"].keys():
        web.write(",%.1f" %( s["disks"][device]["time"] ))
    web.write("]\n")
nchart_line_bot(web, 'Disk Time' + details)
buttonlist.append("Disk-Time")

dstr = ""
for device in jdata[0]["disks"].keys():
    dstr = dstr + "'" + device + "+read','" + device + "-write',"
dstr = dstr[:-1]
nchart_line_top(web, dstr)
for i,s in enumerate(jdata):
    if i == 0:
        continue
    web.write(",['Date(%s)' " %(googledate(s['timestamp']['datetime'])))
    for device in s["disks"].keys():
        web.write(",%.1f,%.1f" %(
                 s["disks"][device]["read_mbps"],
                -s["disks"][device]["write_mbps"]))
    web.write("]\n")
nchart_line_bot(web, 'Disks MB/s' + details)
buttonlist.append("Disk-MB")

dstr = ""
for device in jdata[0]["disks"].keys():
    dstr = dstr + "'" + device + "+read','" + device + "-write',"
dstr = dstr[:-1]
nchart_line_top(web, dstr)
for i,s in enumerate(jdata):
    if i == 0:
        continue
    web.write(",['Date(%s)' " %(googledate(s['timestamp']['datetime'])))
    for device in s["disks"].keys():
        web.write(",%.1f,%.1f " %(
                 s["disks"][device]["read_blks"],
                -s["disks"][device]["write_blks"]))
    web.write("]\n")
nchart_line_bot(web, 'Disk blocks/s' + details)
buttonlist.append("Disk-blocks")

# - - - Adapters
astr = ""
for device in jdata[0]["disk_adapters"].keys():
    astr = astr + "'" + device + "+read KB/s','" + device + "-write KB/s',"
astr = astr[:-1]
nchart_line_top(web, astr)
for i,s in enumerate(jdata):
    if i == 0:
        continue
    web.write(",['Date(%s)' " %(googledate(s['timestamp']['datetime'])))
    for device in s["disk_adapters"].keys():
        web.write(",%.1f,%.1f " %(
                 s["disk_adapters"][device]["read_kb"],
                -s["disk_adapters"][device]["write_kb"]))
    web.write("]\n")
nchart_line_bot(web, 'Adpater KB/s' + details)
buttonlist.append("Disk-Adapt-KBs")

astr = ""
for device in jdata[0]["disk_adapters"].keys():
    astr = astr + "'" + device + "+read OPs','" + device + "-write OPs',"
astr = astr[:-1]
nchart_line_top(web, astr)
for i,s in enumerate(jdata):
    if i == 0:
        continue
    web.write(",['Date(%s)' " %(googledate(s['timestamp']['datetime'])))
    for device in s["disk_adapters"].keys():
        web.write(",%.1f,%.1f " %(
                 s["disk_adapters"][device]["rtransfers"],
                -s["disk_adapters"][device]["wtransfers"]))
    web.write("]\n")
nchart_line_bot(web, 'Adapter OPs' + details)
buttonlist.append("Disk-Adapt-OP")

fsstr = ""
for fs in jdata[0]["filesystems"].keys():
    fsstr = fsstr + "'" + fs + "',"
fsstr = fsstr[:-1]
nchart_line_top(web, fsstr)
for i,s in enumerate(jdata):
    web.write(",['Date(%s)' " %(googledate(s['timestamp']['datetime'])))
    for fs in s["filesystems"].keys():
        web.write(", %.1f" %( s["filesystems"][fs]["used_percent"]))
    web.write("]\n")
nchart_line_bot(web, 'File Systems Used percent' + details)
buttonlist.append("JFS")

graphit(web, "'namei','iget','dirblk'", ni_data,  'Directory lookup' + details, "Namei",unstacked)
graphit(web, "'Incoming', 'Outgoing'", nio_data,  'Network Bytes/s' + details, "TotalNet-Bytes",unstacked)
graphit(web, "'Incoming', 'Outgoing'", np_data,  'Network Packets/s' + details, "TotalNet-Xfer",unstacked)

netstr = ""
for device in jdata[0]["network_interfaces"].keys():
    netstr = netstr + "'" + device + "+in','" + str(device) + "-out',"
netstr = netstr[:-1]
nchart_line_top(web, netstr)
for i,s in enumerate(jdata):
    if i == 0:
        continue
    web.write(",['Date(%s)' " %(googledate(s['timestamp']['datetime'])))
    for device in s["network_interfaces"].keys():
        web.write(",%.1f,%.1f" %(
             s["network_interfaces"][device]["ibytes"],
            -s["network_interfaces"][device]["obytes"]))
    web.write("]\n")
nchart_line_bot(web, 'Network MB/s' + details)
buttonlist.append("Net-MB")

netstr = ""
for device in jdata[0]["network_interfaces"].keys():
    netstr = netstr + "'" + device + "+in','" + str(device) + "-out',"
netstr = netstr[:-1]
nchart_line_top(web, netstr)
for i,s in enumerate(jdata):
    if i == 0:
        continue
    web.write(",['Date(%s)' " %(googledate(s['timestamp']['datetime'])))
    for device in s["network_interfaces"].keys():
        web.write(",%.1f,%.1f " %(
             s["network_interfaces"][device]["ipackets"],
            -s["network_interfaces"][device]["opackets"]))
    web.write("]\n")
nchart_line_bot(web, 'Network packets/s' + details)
buttonlist.append("Net-packets")

graphit(web, "'semaphore','messages'", ipc_data,  'Inter-process commnunication' + details, "IPC",unstacked)

#web.write(config_button_str)
summary = [
"Hostname:" + jdata[0]["identity"]["hostname"],
"Command: " + jdata[0]["identity"]["command"],
"njmon:" + jdata[0]["identity"]["version"],
"User:" + jdata[0]["identity"]["username"],
"DateTime:" + jdata[0]["timestamp"]["datetime"],
"UTC:" + jdata[0]["timestamp"]["UTC"],
"Snapshots:" + str(len(jdata)),
"Seconds:" + str(jdata[0]["timestamp"]["snapshot_seconds"]),
"LPAR:" + jdata[0]["config"]["partitionname"],
"CPU family:" + jdata[0]["config"]["processorFamily"],
"MTM:" + jdata[0]["config"]["processorModel"],
"SerialNo:" + jdata[0]["config"]["machineID"],
"OS:" + jdata[0]["config"]["OSname"] + jdata[0]["config"]["OSversion"],
"SMT:" + str(jdata[0]["config"]["smtthreads"]), 
"Start Processes:" + str(start_processes),
"End Processes:" + str(end_processes) ]

nchart_end(web, hostname,config_str, buttonlist, summary)
web.close()

# The End
