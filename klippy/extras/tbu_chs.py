# Support Check whether the serial channel of TCP bridge UART is duplicated
#
# Copyright (C) 2023  XiaoK <xiaok@zxkxz.cn>
#
# This file may be distributed under the terms of the GNU GPLv3 license.

class PrinterTBU:
    def __init__(self, config):
        self.printer = config.get_printer()
        self.chs = {}
    def check_ch(self, config, serial_ch, tcp_host):
        if tcp_host in self.chs:
            if self.chs[tcp_host] == serial_ch:
                raise config.error("The serial_channel of different MCU on the same TCP bridge cannot be repeated")
        self.chs[tcp_host] = serial_ch
        return serial_ch

def load_config(config):
    return PrinterTBU(config)
