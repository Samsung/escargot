#!/usr/bin/env python

# Copyright 2015-present Samsung Electronics Co., Ltd.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import socket
import select

# pylint: disable=too-many-arguments,superfluous-parens
class TcpSocket(object):
    """ Create a new TCP/IP socket. """
    def __init__(self, address):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM, 0, None)
        if ":" not in address:
            self.address = (address, 6501)
        else:
            host, port = address.split(":")
            self.address = (host, int(port))

    def connect(self):
        """ Connect to a remote socket at address (host, port). """
        print("Connecting to: %s:%s" % (self.address[0], self.address[1]))
        self.socket.connect(self.address)

    def close(self):
        """ Mark the socket closed. """
        self.socket.close()

    def receive_data(self, max_size=1024):
        """ Receive data (maximum amount: max_size). """
        return self.socket.recv(max_size)

    def send_data(self, data):
        """ Send data to the remote peer. """
        return self.socket.send(data)

    def ready(self):
        """ Checks whether data is available. """
        result = select.select([self.socket], [], [], 0)[0]

        return self.socket in result
