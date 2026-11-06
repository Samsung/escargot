#!/usr/bin/env python3
# Copyright 2026-present Samsung Electronics Co., Ltd. and other contributors
# SPDX-License-Identifier: Apache-2.0

"""GCC compiler launcher for Escargot's pretagged Thumb-2 dispatch.

Only ByteCodeInterpreter.cpp is changed. It must opt in with an ELF marker and
pretag every computed-goto target. Remove the redundant state-bit OR and leave
its space after the unconditional BX, where it is never executed. Assemble the
modified source normally so instruction relocations and debug info stay valid.
Leave dispatches across PC-relative instructions intact rather than move them.
This translation unit is compiled without LTO so the linker cannot regenerate
its dispatch. Other translation units keep their original compiler options.
Unexpected compiler output fails the build without leaving an output file.
"""

import argparse
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile


MARKER = ".escargot.thumb_dispatch"
SIGNATURE = b"ESCARGOT_THUMB_DISPATCH_V1"
INTERPRET = "_ZN8Escargot11Interpreter9interpretE"
REGISTER = re.compile(r"\b(r(?:1[0-5]|[0-9])|ip|sl|fp|sp|lr|pc)\b")
ALIASES = {"sl": 10, "fp": 11, "ip": 12, "sp": 13, "lr": 14, "pc": 15}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def register_number(name):
    return ALIASES[name] if name in ALIASES else int(name[1:])


def c_string(data, offset):
    return data[offset:data.index(0, offset)].decode("ascii")


def read_elf(data):
    require(data[:7] == b"\x7fELF\x01\x01\x01", "expected little-endian ELF32")
    require(struct.unpack_from("<HH", data, 16) == (1, 40), "expected a relocatable ARM object")
    section_offset = struct.unpack_from("<I", data, 32)[0]
    entry_size, count, names_index = struct.unpack_from("<HHH", data, 46)
    require(entry_size == 40 and count and names_index < count, "unsupported ELF section table")
    sections = []
    for index in range(count):
        fields = struct.unpack_from("<10I", data, section_offset + index * entry_size)
        section = dict(zip(("name_offset", "type", "flags", "address", "offset", "size",
                            "link", "info", "alignment", "entry_size"), fields))
        require(section["type"] == 8 or section["offset"] + section["size"] <= len(data),
                "ELF section extends beyond the file")
        sections.append(section)
    names = sections[names_index]
    strings = data[names["offset"]:names["offset"] + names["size"]]
    for section in sections:
        section["name"] = c_string(strings, section["name_offset"])
    symbols = []
    for section in sections:
        if section["type"] != 2:
            continue
        require(section["entry_size"] == 16, "unsupported ELF symbol table")
        strings_section = sections[section["link"]]
        strings = data[strings_section["offset"]:strings_section["offset"] + strings_section["size"]]
        for offset in range(section["offset"], section["offset"] + section["size"], 16):
            name, value, size, info, other, index = struct.unpack_from("<IIIBBH", data, offset)
            if name:
                symbols.append((c_string(strings, name), value, size, info, index))
    return sections, symbols


def dispatch_sites(path, data, objdump):
    sections, symbols = read_elf(data)
    markers = [s for s in sections if s["name"] == MARKER]
    require(len(markers) == 1, "missing pretagged-dispatch ELF marker")
    marker = markers[0]
    require(data[marker["offset"]:marker["offset"] + marker["size"]] == SIGNATURE,
            "unknown pretagged-dispatch marker version")
    functions = [s for s in symbols if s[0].startswith(INTERPRET) and s[3] & 15 == 2]
    require(len(functions) == 1, "expected one unpartitioned interpret() function")
    name, value, size, info, section_index = functions[0]
    require(value & 1, "interpret() is not Thumb code")
    section = sections[section_index]
    require(section["flags"] & 4, "interpret() is not in executable code")
    begin, end = value & ~1, (value & ~1) + size
    output = subprocess.check_output(
        [objdump, "-d", "--no-show-raw-insn", "--disassemble=" + name, str(path)],
        env=dict(os.environ, LC_ALL="C"), universal_newlines=True)
    instructions = []
    for line in output.splitlines():
        match = re.match(r"\s*([0-9a-f]+):\s+(\S+)(?:\s+(.*))?$", line)
        if match and begin <= int(match[1], 16) < end:
            instructions.append((int(match[1], 16), match[2], (match[3] or "").split("@")[0].strip()))
    patches = {}
    pc_relative_dispatches = 0
    for index, (address, mnemonic, operands) in enumerate(instructions):
        if mnemonic != "bx":
            continue
        require(REGISTER.fullmatch(operands), "unexpected BX operand")
        target = register_number(operands)
        require(target not in (13, 15), "unexpected BX stack/program-counter operand")
        branch_offset = section["offset"] + address
        require(struct.unpack_from("<H", data, branch_offset)[0] == 0x4700 | (target << 3),
                "unexpected BX instruction encoding")
        found = None
        reads_pc = False
        for previous in reversed(instructions[max(0, index - 16):index]):
            at, operation, arguments = previous
            # Do not trace values across control flow or literal-pool data.
            if operation.startswith(("b", ".", "it", "cb", "tb")):
                break
            if "pc" in REGISTER.findall(arguments):
                reads_pc = True
                break
            used = [register_number(r) for r in REGISTER.findall(arguments)]
            if target not in used:
                continue
            expected = struct.pack("<HH", 0xf040 | target, (target << 8) | 1)
            offset = section["offset"] + at
            if operation == "orr.w" and data[offset:offset + 4] == expected:
                found = offset
            break
        if reads_pc:
            # GCC can schedule PIC setup between the first OR and BX. Moving
            # that instruction changes the PC it reads. Leave this dispatch
            # intact; the pretagged address remains valid with its OR present.
            pc_relative_dispatches += 1
            continue
        if found is None and target == 14:
            continue  # BX LR is an interworking return, not opcode dispatch.
        require(found is not None, "unrecognized dispatch before BX at 0x%x" % address)
        require(found not in patches, "dispatch OR is shared by multiple branches")
        patches[found] = branch_offset
    require(patches, "no redundant Thumb dispatch ORs found")
    if pc_relative_dispatches:
        print("ARM32 dispatch: PC-relative BX sites left unchanged: %d"
              % pc_relative_dispatches)
    for relocation in sections:
        if relocation["type"] not in (4, 9) or relocation["info"] != section_index:
            continue
        require(relocation["entry_size"] in (8, 12), "unsupported relocation entry")
        for offset in range(relocation["offset"], relocation["offset"] + relocation["size"], relocation["entry_size"]):
            address = struct.unpack_from("<I", data, offset)[0] + section["offset"]
            require(not any(p <= address < p + 4 for p in patches), "dispatch OR has a relocation")
    return patches


def rewrite_assembly(source, name, count):
    lines = source.splitlines(keepends=True)
    begin = next(i for i, line in enumerate(lines) if line.strip() == name + ":")
    end = next(i for i in range(begin + 1, len(lines))
               if re.match(r"\s*\.size\s+" + re.escape(name) + r"\s*,", lines[i]))
    instructions = []
    for i in range(begin + 1, end):
        line = lines[i].split("@")[0].strip()
        # Debug labels/directives follow the instructions when reassembled.
        # Any other label may be an entry into this block; do not move code
        # across it, alignment directives, literal pools, or inline assembly.
        if not line or re.match(r"\.L(?:VL|VU|BB|BE|BI)\d+:$", line) or re.match(r"\.(?:loc|cfi_\w+)\b", line):
            continue
        if line.startswith((".", "#")) or line.endswith(":"):
            instructions.append((i, ".barrier", ""))
            continue
        match = re.fullmatch(r"([a-z][a-z0-9.]*)(?:\s+(.+))?", line)
        if match:
            instructions.append((i, match[1], (match[2] or "").strip()))
        else:
            instructions.append((i, ".barrier", ""))
    pairs = {}
    for index, (branch, operation, operands) in enumerate(instructions):
        if operation != "bx":
            continue
        require(REGISTER.fullmatch(operands), "unexpected assembler BX operand")
        target = register_number(operands)
        found = None
        reads_pc = False
        for at, previous, arguments in reversed(instructions[max(0, index - 16):index]):
            if previous.startswith(("b", ".", "it", "cb", "tb")):
                break
            if "pc" in REGISTER.findall(arguments):
                reads_pc = True
                break
            used = [register_number(r) for r in REGISTER.findall(arguments)]
            if target not in used:
                continue
            fields = [field.strip() for field in arguments.split(",")]
            if (previous in ("orr", "orr.w") and len(fields) == 3
                    and used == [target, target] and fields[2] in ("#1", "#0x1")):
                found = at
            break
        if reads_pc or (found is None and target == 14):
            continue
        require(found is not None and found not in pairs, "unrecognized assembler dispatch")
        pairs[found] = branch
    require(len(pairs) == count, "assembler/object dispatch counts differ")
    for at, branch in pairs.items():
        lines[at] = "\t@ Thumb state bit is already set in the opcode table.\n"
        lines[branch] += "\t.space 4, 0 @ Unreachable space preserves subsequent code addresses.\n"
    return "".join(lines)


def assemble_command(command, source, assembly, destination):
    result = []
    skip = False
    for arg in command:
        if skip:
            skip = False
            continue
        if arg in ("-o", "-MF", "-MT", "-MQ"):
            skip = True
            continue
        if arg in ("-MD", "-MMD", "-MP", "-MG"):
            continue
        if not arg.startswith("-") and Path(arg).resolve() == source:
            result.extend(["-x", "assembler", str(assembly)])
        else:
            result.append(arg)
    return result + ["-o", str(destination), "-fno-lto"]


def remove_dispatch_ors(command, source, destination, objdump, objcopy):
    with tempfile.TemporaryDirectory(dir=destination.parent, prefix=".arm32-dispatch-") as directory:
        directory = Path(directory)
        assembly, original, modified = (directory / name for name in ("original.s", "original.o", "modified.o"))
        compile_command = list(command)
        compile_command[compile_command.index("-c")] = "-S"
        compile_command[compile_command.index("-o") + 1] = str(assembly)
        subprocess.check_call(compile_command + ["-DESCARGOT_ARM32_THUMB_DISPATCH=1", "-fno-lto"])
        subprocess.check_call(assemble_command(command, source, assembly, original))
        before = original.read_bytes()
        patches = dispatch_sites(original, before, objdump)
        sections, symbols = read_elf(before)
        function = next(s for s in symbols if s[0].startswith(INTERPRET) and s[3] & 15 == 2)
        assembly.write_text(rewrite_assembly(assembly.read_text(), function[0], len(patches)))
        subprocess.check_call(assemble_command(command, source, assembly, modified))
        after = modified.read_bytes()
        new_sections, new_symbols = read_elf(after)
        new_function = next(s for s in new_symbols if s[0] == function[0] and s[3] & 15 == 2)
        require(function[1:5] == new_function[1:5], "dispatch rewrite changed function layout")
        section, new_section = sections[function[4]], new_sections[new_function[4]]
        require(section["size"] == new_section["size"], "dispatch rewrite changed section size")
        old_code = before[section["offset"]:section["offset"] + section["size"]]
        new_code = bytearray(after[new_section["offset"]:new_section["offset"] + new_section["size"]])
        for at, branch in patches.items():
            at, branch = at - section["offset"], branch - section["offset"]
            require(new_code[branch - 4:branch - 2] == old_code[branch:branch + 2]
                    and new_code[branch - 2:branch + 2] == bytes(4),
                    "dispatch branch or unreachable padding differs from expected output")
            require(new_code[at:branch - 4] == old_code[at + 4:branch],
                    "instructions between the OR and branch changed")
            new_code[at:branch + 2] = old_code[at:branch + 2]
        require(new_code == old_code, "code outside dispatch rewrites changed")
        # Every modified byte is accounted for above. Relocations and DWARF were
        # regenerated by the assembler, not edited at old instruction offsets.
        subprocess.check_call([objcopy, "--remove-section=" + MARKER, str(modified)])
        os.replace(str(modified), str(destination))
        return len(patches)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True)
    parser.add_argument("--objdump", required=True)
    parser.add_argument("--objcopy", required=True)
    parser.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    command = args.command[1:] if args.command[:1] == ["--"] else args.command
    require(command, "missing compiler command")
    source = Path(args.source).resolve()
    if not any(not arg.startswith("-") and Path(arg).name == source.name
               and Path(arg).resolve() == source for arg in command):
        os.execvp(command[0], command)
    require("-c" in command and "-o" in command, "expected a GCC object compilation")
    destination = Path(command[command.index("-o") + 1])
    try:
        count = remove_dispatch_ors(command, source, destination, args.objdump, args.objcopy)
    except Exception:
        if destination.exists():
            destination.unlink()
        raise
    print("ARM32 dispatch: removed %d ORs; preserved layout with unreachable space after BX" % count)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (ValueError, IndexError, StopIteration, struct.error, OSError, subprocess.CalledProcessError) as error:
        print("ARM32 dispatch optimization failed: %s\n"
              "Disable ESCARGOT_ARM32_THUMB_DISPATCH for unsupported compiler output." % error,
              file=sys.stderr)
        sys.exit(1)
