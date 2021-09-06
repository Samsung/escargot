# Internal operation of the Debugger

The debugger uses a client server model where the server is the Escargot engine.
The connection between the client and server must be bi-directional and reliable.

The debugger protocol uses packets, although the connection layer can split
these packets into smaller ones if needed. The maximum packet length is defined by
the connection layer. For example, it is 125 for the websockets layer. When a
message is longer than the maximum packet size, it is split into a sequence of
packets. The type of the last packet has an `_END` prefix. The following
example shows the transmission of strings:

Characters of a string can be 8 or 16 bit long. When a string is transmitted,
it is split into a sequence of packets, and the type of these packets is either
`ESCARGOT_MESSAGE_STRING_8BIT` or `ESCARGOT_MESSAGE_STRING_16BIT`, except
the last one which type is either `ESCARGOT_MESSAGE_STRING_8BIT_END`
or `ESCARGOT_MESSAGE_STRING_16BIT_END`. If a string fits into a single packet,
only the type ending with `_END` is used.

## Debugger modes

The message types which can be transmitted depends on the current mode. These are
the current modes:

* Free running mode: Escargot executes ECMAScript code. The engine may stop at
a breakpoint and notify the client. The client can also request an execution stop.

* Parsing mode: when Escargot parses an ECMAScript code, it produces several
messages which follows the parsing process. For example, Escargot notifies the
client when it starts parsing a new nested function, or the breakpoint list is
available for a function. This reduces the memory consumption, since the data
is collected until the parsing ends. The debugger client sets up its internal
data about the structure of the ECMAScript code based on these messages.

* Breakpoint mode: when Escargot stops at a breakpoint, the client can request
information about the execution status such as backtrace or lexical environment.
The objects and their properties can be enumerated as well. Escargot maintains
a reference to all objects which properties are enumerated until the execution
resumes. Therefore the properties of temporary objects (e.g. the result of a
getter) can be inspected later. These temporary objects may increase the memory
consumption.

* Source sending mode: the client can send multiple source files to Escargot
in this mode, and the engine executes them. Escargot enters this mode when
the user instructs it or it has no code to execute.

## Evaluating code

In breakpoint mode, the client can evaluate ECMAScript code. After the evaluation
is completed, the client receives the string represenation of the result, if the
evaluation is successful, or the string representation of the exception otherwise.
These evaluations can have side effects, e.g. variables or properties can be created,
changed, or deleted. The engine does not maintain a separate environment for
executing code, the side effects remain after the execution resumes. Furthermore
the engine ignores all breakpoints during the evaluation, so infinite loops
will never stop.

## Breakpoints

The Escargot debugger emits disabled breakpoint instructions into the byte code
stream. The debugger client receives the offset of these instructions with their
corresponding line info. Using these offsets the client can request the server
to enable or disable breakpoints by changing the opcode to enabled or disabled
state. Escargot always stops when an enabled breakpoint is executed.
