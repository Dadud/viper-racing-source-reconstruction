#!/usr/bin/env python3
"""Generate PE metadata, x86 Intel disassembly, and printable strings for Viper Racing."""
from pathlib import Path
import argparse, hashlib, re
import pefile
from iced_x86 import Decoder, Formatter, FormatterSyntax

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("install_root",type=Path,help="Extracted game_install directory")
    ap.add_argument("output_dir",type=Path,help="Source repository output directory")
    args=ap.parse_args(); repo=args.output_dir
    targets=[("game",Path("GameRoot/Viper Racing.exe")),("engine",Path("GameBin/race.bin"))]
    report=["# PE binary report",""]; string_report=["# Extracted strings","","Offsets are file offsets. RVAs are mapped through PE section raw/virtual ranges.",""]
    for role,rel in targets:
        path=args.install_root/rel; data=path.read_bytes(); pe=pefile.PE(data=data)
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_IMPORT"]])
        report += [f"## {role}: `{rel.as_posix()}`","",f"- Size: {len(data):,} bytes",f"- SHA-256: `{hashlib.sha256(data).hexdigest()}`",f"- Machine: `0x{pe.FILE_HEADER.Machine:04X}`",f"- Image base: `0x{pe.OPTIONAL_HEADER.ImageBase:X}`",f"- Entry RVA: `0x{pe.OPTIONAL_HEADER.AddressOfEntryPoint:X}`","","| Section | RVA | Virtual size | File offset | Raw size |","|---|---:|---:|---:|---:|"]
        for sec in pe.sections:
            report.append(f"| `{sec.Name.rstrip(bytes([0])).decode('ascii','replace')}` | `0x{sec.VirtualAddress:X}` | `0x{sec.Misc_VirtualSize:X}` | `0x{sec.PointerToRawData:X}` | `0x{sec.SizeOfRawData:X}` |")
        report += ["","### Imports",""]
        for dll in pe.DIRECTORY_ENTRY_IMPORT:
            names=[imp.name.decode('ascii','replace') if imp.name else f"ordinal_{imp.ordinal}" for imp in dll.imports]
            report.append(f"- `{dll.dll.decode('ascii','replace')}`: "+", ".join(f"`{n}`" for n in names))
        for sec in pe.sections:
            if not (sec.Characteristics & 0x20000000): continue
            code=sec.get_data(); va=pe.OPTIONAL_HEADER.ImageBase+sec.VirtualAddress; decoder=Decoder(32,code,ip=va); fmt=Formatter(FormatterSyntax.INTEL); ins=[]
            while decoder.can_decode:
                x=decoder.decode()
                if not x.len: break
                ins.append(x)
            labels={}
            for x in ins:
                if x.flow_control in (3,4,5,6) and va<=x.near_branch_target<va+len(code): labels.setdefault(x.near_branch_target,f"loc_{x.near_branch_target:08X}")
            lines=[]
            for x in ins:
                if x.ip in labels: lines.append(labels[x.ip]+":")
                lines.append(f"{x.ip:08X}: {fmt.format(x)}")
            (repo/"disasm"/f"{role}_{sec.Name.rstrip(bytes([0])).decode('ascii','replace')}.asm").write_text("\n".join(lines)+"\n",encoding="utf-8")
        sections=[(s.PointerToRawData,s.PointerToRawData+s.SizeOfRawData,s.VirtualAddress-s.PointerToRawData,s.Name.rstrip(bytes([0])).decode('ascii','replace')) for s in pe.sections]
        def rva(off):
            for a,b,delta,name in sections:
                if a<=off<b:return f"0x{off+delta:X} ({name})"
            return "n/a"
        strings=[]
        code_ranges=[(sec.PointerToRawData,sec.PointerToRawData+sec.SizeOfRawData) for sec in pe.sections if sec.Characteristics & 0x20000000]
        def in_code(off): return any(a<=off<b for a,b in code_ranges)
        for m in re.finditer(rb"[\x20-\x7e]{5,}",data):
            text=m.group().decode('ascii','replace').strip()
            if text and any(c.isalpha() for c in text) and not in_code(m.start()): strings.append((m.start(),text,"ASCII"))
        for m in re.finditer(rb"(?:[\x20-\x7e]\x00){5,}",data):
            text=m.group().decode('utf-16le','replace').strip()
            if text and any(c.isalpha() for c in text) and not in_code(m.start()): strings.append((m.start(),text,"UTF-16LE"))
        string_report += [f"## {role}: `{rel.as_posix()}` ({len(strings)} strings)","","| File offset | RVA | Encoding | String |","|---:|---:|---|---|"]
        for off,text,encoding in sorted(set(strings)):
            text=text.replace('|','\\|').replace('`','\\`').replace('\n',' ')[:500]
            string_report.append(f"| `0x{off:X}` | `{rva(off)}` | {encoding} | {text} |")
        string_report.append("")
    (repo/"metadata/pe_report.md").write_text("\n".join(report)+"\n",encoding="utf-8")
    (repo/"metadata/strings.md").write_text("\n".join(string_report)+"\n",encoding="utf-8")
if __name__=="__main__": main()
