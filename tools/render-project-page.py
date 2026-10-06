#!/usr/bin/env python3
"""Render the project introduction from its README for GitHub Pages."""

import argparse
import html
import subprocess
import pathlib
import re
import urllib.parse

parser = argparse.ArgumentParser()
parser.add_argument("--output", type=pathlib.Path, required=True)
parser.add_argument("--readme", type=pathlib.Path, required=True)
args = parser.parse_args()
html_file, readme_file = args.output, args.readme
# Keep the release and license badges on the Pages landing page, but
# omit CI, Coverity, and Codecov status badges.
hidden_badges = ("Actions Status", "Coverity Scan Build Status", "codecov")
readme_markdown = "\n".join(
    line for line in readme_file.read_text().splitlines()
    if not any(badge in line for badge in hidden_badges)
)
readme_html = subprocess.check_output(["npx", "--yes", "marked@17.0.3"], input=readme_markdown.encode()).decode("utf-8")
readme_html = re.sub(r'^\s*<h1>Escargot</h1>', '', readme_html, count=1)

def rewrite_links(html):
    def replacer(match):
        attr = match.group(1)
        quote = match.group(2)
        url = match.group(3)
        parsed = urllib.parse.urlparse(url)
        if parsed.scheme or url.startswith('#'):
            return f'{attr}={quote}{url}{quote}'
        clean_url = url.lstrip('./')
        return f'{attr}={quote}https://github.com/Samsung/escargot/blob/master/{clean_url}{quote}'
    pattern = r'(href|src)=(["\'])(.*?)\2'
    return re.sub(pattern, replacer, html)

def add_heading_ids(source):
    used = set()

    def heading(match):
        level, content = match.groups()
        label = html.unescape(re.sub(r'<[^>]+>', '', content)).lower()
        base = re.sub(r'[^\w\- ]', '', label).replace(' ', '-')
        anchor, suffix = base, 0
        while anchor in used:
            suffix += 1
            anchor = f'{base}-{suffix}'
        used.add(anchor)
        return f'<h{level} id="{anchor}">{content}</h{level}>'

    return re.sub(r'<h([1-6])>(.*?)</h\1>', heading, source, flags=re.S)


processed_html = rewrite_links(add_heading_ids(readme_html))

template = f"""<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Escargot</title>
<style>
  body{{margin:0;background:#0b1220;color:#e5edf8;font:16px/1.6 system-ui;line-height:1.6}}
  main{{max-width:900px;margin:8vh auto;padding:32px}}
  .mark{{color:#7dd3fc;font-weight:700;letter-spacing:.08em;text-transform:uppercase}}
  .hero{{margin-bottom:48px}}
  h1{{font-size:48px;margin:.15em 0}}
  .lead{{font-size:20px;line-height:1.6;color:#b9c7da}}
  .links{{display:flex;flex-wrap:wrap;gap:12px;margin-top:28px}}
  a{{color:#7dd3fc;text-decoration:none}}
  .links a{{background:#17243a;border:1px solid #304664;border-radius:10px;padding:12px 16px;color:#7dd3fc;text-decoration:none}}
  .links a:hover{{background:#213451}}
  .readme {{margin-top:64px;border-top:1px solid #304664;padding-top:48px}}
  .readme h1, .readme h2, .readme h3, .readme h4, .readme h5, .readme h6 {{color:#7dd3fc;margin-top:1.5em;margin-bottom:0.5em}}
  .readme h1 {{font-size:32px;border-bottom:1px solid #304664;padding-bottom:8px}}
  .readme h2 {{font-size:24px;border-bottom:1px solid #22344e;padding-bottom:6px}}
  .readme h3 {{font-size:20px}}
  .readme p {{margin:1em 0}}
  .readme a {{color:#38bdf8;text-decoration:underline}}
  .readme a:hover {{color:#7dd3fc}}
  .readme pre {{background:#17243a;border:1px solid #304664;border-radius:8px;padding:16px;overflow-x:auto;font-family:monospace}}
  .readme code {{font-family:monospace;background:#17243a;padding:2px 6px;border-radius:4px;font-size:0.9em}}
  .readme pre code {{background:none;padding:0;border-radius:0;font-size:1em}}
  .readme table {{width:100%;border-collapse:collapse;margin:24px 0}}
  .readme th, .readme td {{border:1px solid #304664;padding:10px;text-align:left}}
  .readme th {{background:#17243a;color:#7dd3fc}}
  .readme blockquote {{border-left:4px solid #7dd3fc;margin:0;padding-left:16px;color:#b9c7da}}
  .readme ul, .readme ol {{padding-left:24px}}
  .readme li {{margin-bottom:8px}}
  .readme img {{max-width:100%;height:auto}}
</style>
<main>
  <div class="hero">
    <div class="mark">Embeddable JavaScript engine</div>
    <h1>Escargot</h1>
    <p class="lead">Modern JavaScript for memory-constrained applications.</p>
    <div class="links">
      <a href="#quick-start">Quick start</a>
      <a href="https://github.com/Samsung/escargot">GitHub repository</a>
      <a href="performance/monthly/">Compare engines</a>
      <a href="performance/">Performance history</a>
    </div>
  </div>
  <div class="readme">
    {processed_html}
  </div>
</main>
</html>
"""
html_file.write_text(template, encoding="utf-8")
