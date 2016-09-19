import subprocess
import time
import logging
import psutil
import re
import matplotlib.pyplot as plt
import argparse

def _remove_rate(interface):

    cmd = ["sudo",
        "tc", "qdisc", "del", "dev", interface, "root"]

    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

def _change_bitrate(logger, interface, bitrate):

    logger.warning("setting bitrate %s", bitrate)
    cmd = ["sudo",
        "tc", "qdisc", "add", "dev", interface, "root", "tbf", "rate",
        bitrate+"kbit", "latency", "50ms", "burst", bitrate+"b"]

    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)

def _init_bitrate_list(first_val, last_val, inc):

    return range(first_val, last_val, inc)

def test_network(logger, interface, time_between_test, path_image):

    logger.warning("running test_case_network_going_down on %s", interface)

    max = 5000
    min = 2000
    inc = 200

    list_total_dropped = []
    list_bits_sent_per_sec = []
    list_bitrate = _init_bitrate_list(max, min, -inc)

    bits_sent_offset = _add_eth_stats(logger, interface, 0, 0, list_bits_sent_per_sec)

    for bitrate in list_bitrate:
        _change_bitrate(logger, interface, str(bitrate))
        time.sleep(time_between_test)

        _add_tc_stats(logger, interface, list_total_dropped)
        bits_sent_offset = _add_eth_stats(logger, interface, bits_sent_offset, time_between_test, list_bits_sent_per_sec)

        _remove_rate(interface)

    fig, ax = plt.subplots()
    plt.plot(list_bitrate, ':b', label='bitrate')
    plt.plot(list_total_dropped, 'r-', label='pkt dropped')
    plt.plot(list_bits_sent_per_sec, 'g-', label='bits sent')
    plt.grid(True)
    legend = plt.legend()

    if path_image:
        plt.savefig(path_image)
    else:
        plt.show()


def _add_tc_stats(logger, interface, list_total_dropped):

    cmd = ["tc", "-s", "qdisc", "show", "dev", interface]
    stdout = subprocess.check_output(cmd)
    m = re.search('dropped (.+?),', stdout)
    if m:
        list_total_dropped.append(int(m.group(1)))
    else:
        list_total_dropped.append(0)

def _add_eth_stats(logger, interface, offset, period, list_bits_sent_per_sec):

    ret = psutil.net_io_counters(pernic=True)[interface][0]
    if period != 0:
        rate = 8 * (ret - offset) / (period * 1000.0)
        list_bits_sent_per_sec.append(rate)

    return ret

def init_logger():

    logger = logging.getLogger('test_network')
    logger.setLevel(logging.WARNING)
    steam_handler = logging.StreamHandler()
    steam_handler.setLevel(logging.WARNING)
    logger.addHandler(steam_handler)

    return logger

def main():

    logger = init_logger()
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', '--interface', help='interface', required=True)
    parser.add_argument('-d', '--delay', help='time between test', required=True)
    parser.add_argument('-w', '--write', help='path to save images')
    args = parser.parse_args()

    _remove_rate(args.interface)
    test_network(logger, args.interface, int(args.delay), args.write)

if __name__ == '__main__':
    main()
