#!/usr/bin/env python3
"""Publish validated engine comparisons as HTML and a README-compatible SVG."""

import argparse
import html
import json
import math
from pathlib import Path
import re
import zipfile

ENGINES = ("quickjs", "d8", "d8_jitless", "escargot")
NAMES = {"quickjs": "QuickJS", "d8": "V8 d8", "d8_jitless": "V8 d8 --jitless", "escargot": "Escargot"}
SUITES = ("sunspider", "octane", "web_tooling")
TITLES = {"sunspider": "SunSpider", "octane": "Octane", "web_tooling": "Web Tooling"}
METHOD = "fixed-work-smaps-rollup-v2"


def architecture(data):
    value = data.get("architecture")
    if value in ("aarch64", "arm64"):
        return "arm64"
    if value in ("arm", "arm32", "armv7l", "armv8l"):
        return "arm32"
    raise ValueError(f"Unsupported comparison architecture: {value}")


def validate(data):
    arch = architecture(data)
    if arch == "arm32" and (data.get("pointer_bits") != 32 or data.get("execution_mode") != "native"):
        raise ValueError("ARM32 comparisons require verified native 32-bit engines")
    if not data.get("memory_validated") or data.get("memory_method") != METHOD:
        raise ValueError("Only validated fixed-work memory comparisons may be published")
    signatures = {}
    expected_work = {"sunspider": (26, 1), "octane": (18, 1), "web_tooling": (18, 3)}
    for engine in ENGINES:
        for suite in SUITES:
            memory = data["engines"][engine]["memory"][suite]
            if "error" in memory or len(memory["samples"]) != 3:
                raise ValueError(f"Incomplete measurement: {engine}/{suite}")
            for sample in memory["samples"]:
                if sample["method"] != METHOD or sample["cpu_affinity"] != [str(data["cpu"])]:
                    raise ValueError("Measurement method or CPU mismatch")
                for key in ("average_rss_kib", "peak_rss_kib", "average_pss_kib", "average_uss_kib"):
                    if not math.isfinite(sample[key]) or sample[key] <= 0:
                        raise ValueError("Invalid resident-memory value")
                count, iterations = expected_work[suite]
                if len(sample["work_counts"]) != count or any(value != iterations for value in sample["work_counts"].values()):
                    raise ValueError("Incorrect fixed-work counts")
                labels = [point["label"] for point in sample["checkpoints"]]
                if labels[:2] != ["baseline", "loaded"] or labels[-2:] != ["workload_end", "after_gc"]:
                    raise ValueError("Incomplete memory checkpoints")
                if sample["monitor_cpu_affinity"] == sample["cpu_affinity"] or sample["rss_sample_count"] < 1:
                    raise ValueError("Missing independent memory sampling")
                signature = sample["work_signature"]
                if suite in signatures and signatures[suite] != signature:
                    raise ValueError("Inconsistent work counts")
                signatures[suite] = signature
            score = data["engines"][engine].get(suite, {})
            key = {"sunspider": "total_milliseconds", "octane": "score", "web_tooling": "score_runs_per_second"}[suite]
            if key not in score or not math.isfinite(score[key]) or score[key] <= 0:
                raise ValueError(f"Missing performance result: {engine}/{suite}")


def version(data, engine):
    if engine == "escargot":
        return data["escargot_source_revision"][:9]
    return data["engines"][engine]["version"]


def scores(data, engine):
    e = data["engines"][engine]
    return [f"{e['sunspider']['total_milliseconds']:.1f}",
            f"{e['octane']['score']:,.0f}", f"{e['web_tooling']['score_runs_per_second']:.2f}"]


def svg(data):
    label = architecture(data).upper()
    elements = ['<svg xmlns="http://www.w3.org/2000/svg" width="1080" height="600" viewBox="0 0 1080 600" role="img" aria-labelledby="title desc">',
                f'<title id="title">{label} JavaScript engine performance and fixed-work memory</title>',
                '<desc id="desc">Median of three memory runs. Memory uses RSS from fixed work; execution scores are measured separately.</desc>',
                '<rect width="1080" height="600" rx="16" fill="#0b1220"/>']
    def text(x, y, value, size=16, color="#e5edf8", weight="normal"):
        elements.append(f'<text x="{x}" y="{y}" fill="{color}" font-family="Arial, sans-serif" font-size="{size}" font-weight="{weight}">{html.escape(str(value))}</text>')
    text(28, 40, f"JavaScript engines on {label}", 26, weight="bold")
    text(28, 70, f"Measured {data['measured_date_kst']} | Escargot {data['escargot_source_revision'][:9]} | CPU {data['cpu']}", 15, "#b9c7da")
    x = [28, 255, 475, 685, 865]
    text(28, 110, "Fixed-work memory: average / peak RSS (MiB)", 20, "#7dd3fc", "bold")
    for col, value in enumerate(["Engine", "Version", "SunSpider", "Octane", "Web Tooling"]):
        text(x[col], 142, value, 16, "#b9c7da", "bold")
    for index, engine in enumerate(ENGINES):
        y = 178 + index * 38
        if engine == "escargot":
            elements.append(f'<rect x="14" y="{y-25}" width="1052" height="35" rx="5" fill="#17324b"/>')
        values = [NAMES[engine], version(data, engine)]
        values += [f"{data['engines'][engine]['memory'][s]['average_rss_kib']/1024:.1f} / {data['engines'][engine]['memory'][s]['peak_rss_kib']/1024:.1f}" for s in SUITES]
        for col, value in enumerate(values):
            text(x[col], y, value, 16, weight="bold" if engine == "escargot" else "normal")
    text(28, 351, "Execution performance (separate score runs)", 20, "#7dd3fc", "bold")
    for col, value in enumerate(["Engine", "", "SunSpider ms ↓", "Octane score ↑", "WTB runs/s ↑"]):
        text(x[col], 382, value, 15, "#b9c7da", "bold")
    for index, engine in enumerate(ENGINES):
        y = 418 + index * 32
        if engine == "escargot":
            elements.append(f'<rect x="14" y="{y-23}" width="1052" height="31" rx="5" fill="#17324b"/>')
        for col, value in enumerate([NAMES[engine], "", *scores(data, engine)]):
            text(x[col], y, value, 16, weight="bold" if engine == "escargot" else "normal")
    text(28, 555, "Memory: median of 3 fresh processes; loading and natural GC included. Kernel peak RSS.", 14, "#b9c7da")
    text(28, 580, "Same CPU, engine versions and work counts. Click for PSS/USS, ranges, methods and raw data.", 14, "#b9c7da")
    elements.append('</svg>')
    return '\n'.join(elements) + '\n'


def page(embed=False, arm32=False, default_architecture="arm64"):
    css = '''body{margin:0;background:#0b1220;color:#e5edf8;font:16px/1.5 system-ui}main{max-width:1120px;margin:auto;padding:24px}h1{font-size:28px;margin:0 0 8px}h2{font-size:21px}a{color:#7dd3fc}p{color:#b9c7da}table{width:100%;border-collapse:collapse;font-size:15px}th,td{padding:11px 12px;text-align:right;border-bottom:1px solid #304664;white-space:nowrap}th:first-child,td:first-child{text-align:left}.tables{overflow-x:auto;background:#17243a;border:1px solid #304664;border-radius:12px}.escargot{background:#17324b;font-weight:650}select{padding:7px;background:#17243a;color:#e5edf8;border:1px solid #304664;border-radius:6px}details{margin-top:24px}summary{cursor:pointer}small{font-size:13px;color:#b9c7da}.controls{display:flex;gap:12px;align-items:center;flex-wrap:wrap}ul{color:#b9c7da;padding-left:22px}#status{color:#fca5a5}'''
    css += '.architectures{display:flex;gap:8px;margin:16px 0}.architectures button{font:inherit;padding:7px 18px;border:1px solid #304664;border-radius:6px;background:#17243a;color:#e5edf8;cursor:pointer}.architectures button[aria-pressed="true"]{background:#17324b;border-color:#7dd3fc;font-weight:650}'
    paths = {"arm64": "../" if default_architecture == "arm32" else ""}
    if arm32:
        paths["arm32"] = "" if default_architecture == "arm32" else "arm32/"
    script = 'const architecturePaths=' + json.dumps(paths) + ';\nconst defaultArchitecture=' + json.dumps(default_architecture) + ';\n' + r'''
const names={quickjs:'QuickJS',d8:'V8 d8',d8_jitless:'V8 d8 --jitless',escargot:'Escargot'}, engines=Object.keys(names), suites=['sunspider','octane','web_tooling'];
let data, requestId=0;
const median=a=>{a=[...a].sort((a,b)=>a-b);return a[Math.floor(a.length/2)];};
const mib=x=>(x/1024).toFixed(1);
function addRow(parent,values,engine){const tr=document.createElement('tr');if(engine==='escargot')tr.className='escargot';for(const value of values){const td=document.createElement('td');td.textContent=value;tr.append(td);}parent.append(tr);}
function memory(){if(!data)return;const metric=document.getElementById('metric').value;const body=document.getElementById('memory');body.replaceChildren();for(const name of engines){const cells=[names[name]];for(const suite of suites){const m=data.engines[name].memory[suite], samples=m.samples;let avg,peak;
if(metric==='after_gc'){cells.push(mib(median(samples.map(s=>s.checkpoints.find(c=>c.label==='after_gc').uss_kib))));continue;}
avg=m['average_'+metric+'_kib'];peak=metric==='rss'?m.peak_rss_kib:median(samples.map(s=>s['sampled_peak_'+metric+'_kib']));cells.push(mib(avg)+' / '+mib(peak));}addRow(body,cells,name);}
document.getElementById('memory-description').textContent=metric==='after_gc'?'Private resident memory at the separate post-GC checkpoint, MiB.':(metric==='rss'?'Time-weighted average / kernel lifetime peak RSS, MiB.':'Time-weighted average / sampled peak '+metric.toUpperCase()+', MiB.');resize();}
function resize(){if(parent!==window)parent.postMessage({type:'escargot-comparison-height',height:document.documentElement.scrollHeight},location.origin);}
async function comparison(arch){const id=++requestId,base=architecturePaths[arch];data=undefined;
for(const name of ['memory','scores','ranges','subtests'])document.getElementById(name)?.replaceChildren();
document.getElementById('heading').textContent='JavaScript engines on '+arch.toUpperCase();
document.getElementById('context').textContent='Loading validated comparison…';document.getElementById('status').textContent='';
document.getElementById('architecture-description').textContent=arch==='arm32'?'32-bit embedded environments. Native execution on the ARM64 benchmark server.':'64-bit ARM environments.';
document.getElementById('score-date').textContent='';document.getElementById('memory-description').textContent='';
for(const button of document.querySelectorAll('[data-architecture]'))button.setAttribute('aria-pressed',String(button.dataset.architecture===arch));
try{const response=await fetch(base+'latest.json',{cache:'no-store'});if(!response.ok)throw Error('Results unavailable');const d=await response.json();if(id!==requestId)return;
const measuredArch=['arm','arm32','armv7l','armv8l'].includes(d.architecture)?'arm32':'arm64';if(measuredArch!==arch)throw Error('Result architecture mismatch');
data=d;document.getElementById('context').textContent='Memory measured '+d.measured_at.slice(0,19).replace('T',' ')+' UTC · '+arch.toUpperCase()+' CPU '+d.cpu+' · Escargot '+d.escargot_source_revision.slice(0,9)+' · 3 memory runs per configuration';
for(const name of engines){const e=d.engines[name];addRow(document.getElementById('scores'),[names[name],name==='escargot'?d.escargot_source_revision.slice(0,9):e.version,e.sunspider.total_milliseconds.toFixed(1),e.octane.score.toLocaleString('en-US'),e.web_tooling.score_runs_per_second.toFixed(2)],name);}
for(const name of engines){for(const suite of suites){const m=d.engines[name].memory[suite];addRow(document.getElementById('ranges'),[names[name],suite,m.average_rss_kib_range.map(mib).join(' – '),m.peak_rss_kib_range.map(mib).join(' – ')]);}}
document.getElementById('score-date').textContent='Score run: '+(d.score_measured_at||d.measured_at).slice(0,19).replace('T',' ')+' UTC. SunSpider: mean of 5 fresh-process runs; Octane: one score run; WTB: minSamples=3.';
document.getElementById('raw-json').href=base+d.published_raw_json;document.getElementById('raw-zip').href=base+d.published_raw_archive;
document.getElementById('full-comparison').href=base||'./';const history=document.getElementById('history');if(history)history.href=base+'history.json';
const tests=document.getElementById('subtests');if(tests){for(const [suite,key] of [['web_tooling','subtests_runs_per_second'],['octane','subtests_scores']]){const names=Object.keys(d.engines.d8[suite][key]);for(const test of names){addRow(tests,[suite,test,...engines.map(name=>d.engines[name][suite][key][test].toFixed(2))]);}}}
memory();}catch(e){if(id===requestId){document.getElementById('status').textContent=e.message;resize();}}}
for(const button of document.querySelectorAll('[data-architecture]'))button.addEventListener('click',()=>comparison(button.dataset.architecture));
comparison(defaultArchitecture);
document.getElementById('metric').addEventListener('change',memory);window.addEventListener('resize',resize);
'''
    methods = '''<details><summary>Measurement method and interpretation</summary><ul>
<li>Memory uses three fresh processes per engine and suite, rotating engine order. The table shows medians; ranges appear below.</li>
<li>Within each architecture, engines use the same native ARM host, engine CPU, glibc runtime and fixed work: SunSpider 26 scripts once; Octane 18 benchmark functions once, including normal setup/teardown; Web Tooling 18 functions three times. ARM32 uses an Ubuntu ARM32 container without emulation.</li>
<li>Averages cover loading and execution with natural GC, from the baseline checkpoint to workload end. Checkpoint waiting time is excluded; no manual GC between subtests.</li>
<li>RSS/PSS/private resident memory is read externally from Linux smaps_rollup every 10 ms, on a separate monitor CPU. Averages use actual timestamps. All threads are stopped for phase snapshots.</li>
<li>Peak RSS uses GNU time / wait4 ru_maxrss over the whole process lifetime, including the post-GC checkpoint. PSS/USS peaks are sampled observations.</li>
<li>PSS apportions shared resident pages; USS is Private_Clean plus Private_Dirty. The separate post-GC view follows two explicit GC requests; it is not a JS heap-size measurement.</li>
<li>These are shell process measurements on these workloads. They do not predict a complete application's or browser's memory use.</li>
<li>ARM32 and ARM64 results retain their own dates, engine versions and runtime details. Differences between the tables are not solely the effect of pointer width and do not represent every ARM device.</li>
</ul></details>'''
    extra = '' if embed else '''<details><summary>Individual performance tests</summary><div class="tables"><table><thead><tr><th>Suite</th><th>Test</th><th>QuickJS</th><th>d8</th><th>d8 --jitless</th><th>Escargot</th></tr></thead><tbody id="subtests"></tbody></table></div></details><p><a id="history" href="history.json">Measurement history JSON</a></p>'''
    buttons = ''.join(f'<button type="button" data-architecture="{arch}" aria-pressed="{str(arch == default_architecture).lower()}">{arch.upper()}</button>' for arch in paths)
    return f'''<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Escargot engine comparison</title><style>{css}</style></head><body><main>
<h1 id="heading">JavaScript engines on {default_architecture.upper()}</h1><nav class="architectures" aria-label="Benchmark architecture">{buttons}</nav><p id="architecture-description"></p><p id="context">Loading validated comparison…</p><p id="status" role="status"></p>
<h2>Memory on the same fixed work</h2><div class="controls"><label for="metric">Memory metric</label><select id="metric"><option value="rss">RSS</option><option value="pss">PSS (proportional)</option><option value="uss">USS (private)</option><option value="after_gc">USS after GC</option></select><small id="memory-description"></small></div>
<div class="tables"><table><thead><tr><th>Engine</th><th>SunSpider</th><th>Octane</th><th>Web Tooling</th></tr></thead><tbody id="memory"></tbody></table></div>
<h2>Execution performance</h2><div class="tables"><table><thead><tr><th>Engine</th><th>Version</th><th>SunSpider ms ↓</th><th>Octane score ↑</th><th>WTB runs/s ↑</th></tr></thead><tbody id="scores"></tbody></table></div><p><small id="score-date"></small></p>
<p><a id="raw-json" href="latest.json">Raw measurements JSON</a> · <a id="raw-zip" href="#">All logs, samples and checkpoints</a> · <a id="full-comparison" href="./" target="_top">Full comparison</a></p>
{methods}<details><summary>Ranges across three memory runs (MiB)</summary><div class="tables"><table><thead><tr><th>Engine</th><th>Suite</th><th>Average RSS range</th><th>Kernel peak RSS range</th></tr></thead><tbody id="ranges"></tbody></table></div></details>{extra}
</main><script>{script}</script></body></html>\n'''


def inject_landing(path):
    path = Path(path)
    text = path.read_text()
    iframe = '<iframe title="Latest JavaScript engine performance and memory comparison" src="performance/monthly/embed.html" style="width:100%;height:800px;border:0" loading="lazy"></iframe>'
    pattern = r'<p><a[^>]*><img[^>]*src="https://samsung.github.io/escargot/performance/monthly/latest.svg"[^>]*></a></p>'
    text, count = re.subn(pattern, iframe, text)
    if not count and 'performance/monthly/embed.html' not in text:
        text = text.replace('<div class="readme">', '<section><h2>Performance and memory</h2>' + iframe + '</section><div class="readme">', 1)
    if 'escargot-comparison-height' not in text:
        text = text.replace('</html>', '''<script>window.addEventListener('message',event=>{if(event.origin!==location.origin||event.data?.type!=='escargot-comparison-height')return;const frame=document.querySelector('iframe[src="performance/monthly/embed.html"]');if(frame&&event.source===frame.contentWindow&&Number.isFinite(event.data.height))frame.style.height=Math.min(6000,Math.max(300,event.data.height))+'px';});</script></html>''')
    path.write_text(text)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--report', type=Path)
    parser.add_argument('--site', type=Path)
    parser.add_argument('--inject-landing', type=Path)
    args = parser.parse_args()
    if args.report:
        if not args.site:
            parser.error('--site is required with --report')
        data = json.loads((args.report / 'measurements.json').read_text())
        validate(data)
        comparison_root = args.site / 'performance/monthly'
        arch = architecture(data)
        root = comparison_root / 'arm32' if arch == 'arm32' else comparison_root
        raw = root / 'raw'
        raw.mkdir(parents=True, exist_ok=True)
        identifier = data['measured_date_kst'] + '-' + data['escargot_revision'][:9]
        data['published_raw_json'] = 'raw/' + identifier + '.json'
        data['published_raw_archive'] = 'raw/' + identifier + '.zip'
        (raw / (identifier + '.json')).write_text(json.dumps(data, indent=2) + '\n')
        with zipfile.ZipFile(raw / (identifier + '.zip'), 'w', zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(args.report.rglob('*')):
                if path.is_file():
                    archive.write(path, path.relative_to(args.report))
        (root / 'latest.json').write_text(json.dumps(data, indent=2) + '\n')
        (root / 'latest.svg').write_text(svg(data))
        history_path = root / 'history.json'
        history = json.loads(history_path.read_text()) if history_path.exists() else []
        history = [row for row in history if row.get('memory_method') == METHOD and row.get('measured_at') != data['measured_at']]
        history.append(data)
        history_path.write_text(json.dumps(history[-120:], indent=2) + '\n')
        has_arm32 = (comparison_root / 'arm32/latest.json').is_file()
        if (comparison_root / 'latest.json').is_file():
            (comparison_root / 'index.html').write_text(page(arm32=has_arm32))
            (comparison_root / 'embed.html').write_text(page(embed=True, arm32=has_arm32))
        if has_arm32:
            (comparison_root / 'arm32/index.html').write_text(page(arm32=True, default_architecture='arm32'))
            (comparison_root / 'arm32/embed.html').write_text(page(embed=True, arm32=True, default_architecture='arm32'))
    if args.inject_landing:
        inject_landing(args.inject_landing)


if __name__ == '__main__':
    main()
