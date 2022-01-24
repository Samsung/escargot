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

import struct

MAX_BUFFER_SIZE = 128
WEBSOCKET_BINARY_FRAME = 2
WEBSOCKET_FIN_BIT = 0x80

class WebSocket(object):
    def __init__(self, transport):

        self.data_buffer = b""
        self.transport = transport

    def connect(self):
        """  WebSockets connection. """
        self.transport.connect()
        self.receive_buffer = b""

        self.__send_data(b"GET /escargot-debugger HTTP/1.1\r\n" +
                         b"Upgrade: websocket\r\n" +
                         b"Connection: Upgrade\r\n" +
                         b"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n")

        # Expected answer from the handshake.
        expected = (b"HTTP/1.1 101 Switching Protocols\r\n" +
                    b"Upgrade: websocket\r\n" +
                    b"Connection: Upgrade\r\n" +
                    b"Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n\r\n")

        len_expected = len(expected)

        while len(self.receive_buffer) < len_expected:
            self.receive_buffer += self.transport.receive_data()

        if self.receive_buffer[0:len_expected] != expected:
            raise Exception("Unexpected handshake")

        if len(self.receive_buffer) > len_expected:
            self.receive_buffer = self.receive_buffer[len_expected:]
        else:
            self.receive_buffer = b""


        # First the version info must be returned
        # header [2] - opcode[1], size[1]
        # MessageVersion [6 bytes]
        len_expected = 2 + 6

        while len(self.receive_buffer) < len_expected:
            self.receive_buffer += self.transport.receive_data()

        expected = struct.pack("BB",
                               WEBSOCKET_BINARY_FRAME | WEBSOCKET_FIN_BIT,
                               6)

        if self.receive_buffer[0:2] != expected:
            raise Exception("Unexpected version info")

        result = self.receive_buffer[2:len_expected]
        self.receive_buffer = self.receive_buffer[len_expected:]

        return result

    def __send_data(self, data):
        """ Private function to send data using the given transport. """
        size = len(data)

        while size > 0:
            bytes_send = self.transport.send_data(data)
            if bytes_send < size:
                data = data[bytes_send:]
            size -= bytes_send

    def send_message(self, byte_order, packed_data):
        """ Send message. """
        message = struct.pack(byte_order + "BBI",
                              WEBSOCKET_BINARY_FRAME | WEBSOCKET_FIN_BIT,
                              WEBSOCKET_FIN_BIT + struct.unpack(byte_order + "B", bytes([packed_data[0]]))[0],
                              0) + packed_data[1:]

        self.__send_data(message)

    def close(self):
        """ Close the WebSocket. """
        self.transport.close()

    def get_message(self, blocking):
        """ Receive message. """

        # Connection was closed
        if self.receive_buffer is None:
            return None

        while True:
            if len(self.receive_buffer) >= 2:
                if self.receive_buffer[0] != WEBSOCKET_BINARY_FRAME | WEBSOCKET_FIN_BIT:
                    raise Exception("Unexpected data frame")

                size = self.receive_buffer[1]
                if size == 0 or size >= 126:
                    raise Exception("Unexpected data frame")

                if len(self.receive_buffer) >= size + 2:
                    result = self.receive_buffer[2:size + 2]
                    self.receive_buffer = self.receive_buffer[size + 2:]
                    return result

            if not blocking and not self.transport.ready():
                return b''

            data = self.transport.receive_data(MAX_BUFFER_SIZE)

            if not data:
                self.receive_buffer = None
                return None
            self.receive_buffer += data
