#!/usr/bin/python
#
# Author : Changqian Wang(changqwa@cisco.com)
# Description : Collect traffic telemetry from cbr8 for special mac-domains.
#

import sys
import time
import telnetlib
import re
import datetime
import os
import sched

try:
    import json
except ImportError:
    import simplejson as json

#############################Global Variable Section Begin##########################
json_data = None
#############################Global Variable Section End############################

def ConvertToAscII(input_data):
    if isinstance(input_data, dict):
        return dict((ConvertToAscII(key), ConvertToAscII(value)) for key, value in input_data.iteritems())
    elif isinstance(input_data, list):
        return [ConvertToAscII(element) for element in input_data]
    elif isinstance(input_data, unicode):
        return input_data.encode('ascii')
    else:
        return input_data
    
class TelnetProcessor:
    def __init__(self):
        tb_info = json_data["Testbed"]
        self.__tb_ip = tb_info["ip"]
        self.__tb_user = tb_info["user"]
        self.__tb_pd = tb_info["password"]
        self.__tb_prompt = tb_info["prompt"]
        self.__Open()
        
    def __Open(self):
        self.__tn = telnetlib.Telnet(self.__tb_ip, port=23, timeout=10) 
        self.__tn.read_until('Username: ')  
        self.__tn.write(self.__tb_user + '\n')  
        self.__tn.read_until('Password: ')  
        self.__tn.write(self.__tb_pd + '\n')         
        self.__tn.expect([self.__tb_prompt])  
        
    def ExecuteCmd(self, cmd, prompt = None, rt_char = True):
        if not prompt:
            prompt = self.__tb_prompt
        if rt_char:
            cmd += "\n"
        self.__tn.write(cmd) 
        _, _, response = self.__tn.expect([prompt])
        return response
    
    def Close(self):
        self.__tn.close()

def ObtainNumber(text, pos):
    num_pattern = "r[-+]?\d*\.\d+|\d+"
    num_strs = re.findall(num_pattern, text)
    if abs(pos) >= len(num_strs):
        return 0
    return int(num_strs[pos])

class PeriodScheduler():
    def __init__(self):
        self.__sched = sched.scheduler(time.time, time.sleep)

    def DisplayTimeStamp(self):
        print datetime.datetime.fromtimestamp(time.time()).strftime('%Y%m%d-%H%M%S')
    
    # This api guarantee func will be invoked on the coming time point which is divisible by the period.
    def Enter(self, period, priority, func, args):
        now = int(time.time())
        if period == 0:
            time_point = now
        else:
            time_point = (now/period + 1) * period
        self.__sched.enterabs(time_point, priority, func, args)    

    # This api guarantee func will be invoked exactly on the time point.        
    def EnterTimePoint(self, time_point, priority, func, args):
        self.__sched.enterabs(time_point, priority, func, args)    
    
    def Run(self):
        self.__sched.run()
            
class TestbedController:
    def __init__(self, scheduler):
        self.__tp = TelnetProcessor()
        self.__mds = []
        self.__sched = scheduler

    def __CheckUsChanUtilization(self):
        wasted_bands = []
        for md in self.__mds:
            text = self.__tp.ExecuteCmd("show interfaces cable %s mac-scheduler | i data grants" %md)
            percent = ObtainNumber(text.split(":")[1], 0)
            wasted_bands.append(26.0*percent/100 - 11)
        content = "Normal %f\nWaterfall %f\nEND" %(wasted_bands[0], wasted_bands[1])
        
        outfile = open(json_data["OutputFile"], 'w')
        outfile.write(content)
        outfile.close()
                        
    def MonitorChanUtilization(self):   
        self.__sched.DisplayTimeStamp()
        self.__CheckUsChanUtilization()
        self.__sched.Enter(int(json_data["Period"]["channel"]), 2, TestbedController.MonitorChanUtilization, [self])
        
    def MonitorTelemetryByScheduler(self, mds):
        self.__mds = mds
        self.MonitorChanUtilization()
        self.__sched.Run()

def ReadConfig():
    env_conf = os.path.dirname(os.path.abspath(__file__))  + "/lld_conf.json"
#     print env_conf
    global json_data
    try:
        with open(env_conf, "r") as json_file:
            json_data = ConvertToAscII(json.load(json_file))
    except (IOError, ValueError) as detail:
        print "fail to open lld_conf.json"
        print "Error:", detail
        sys.exit(-1)
    except:
        print "fail to open lld_conf.json"
        print "UnknownError"
        sys.exit(-1)

def PortTraffic():
    os.system("killall -9 owtraffic")
    os.system("../owtraffic/owtraffic --client-ns cpe100 --client-ip 105.29.1.66 --server-port 44444"
              " --packet-size 250 --packet-num 5000 --interval 1900 --output-file data_nh.txt&")
    os.system("../owtraffic/owtraffic --client-ns cpe100 --client-ip 105.29.1.66 --server-port 44443"
              " --packet-size 250 --packet-num 5000 --interval 1900 --output-file data_nl.txt&")

    os.system("../owtraffic/owtraffic --client-ns cpe102 --client-ip 105.29.1.88 --server-port 44442"
              " --packet-size 250 --packet-num 5000 --interval 1900 --output-file data_wh.txt&")
    os.system("../owtraffic/owtraffic --client-ns cpe102 --client-ip 105.29.1.88 --server-port 44441"
              " --packet-size 250 --packet-num 5000 --interval 1900 --output-file data_wl.txt&")
    
def main(argv):
    if len(argv) > 1:
        print "Usage: python %s" %argv[0]
        sys.exit(-1)
    ReadConfig()
 
    PortTraffic()
    TestbedController(PeriodScheduler()).MonitorTelemetryByScheduler(json_data["MacDomain"])

if __name__ == '__main__':
    main(sys.argv[:])
