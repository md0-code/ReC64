"""Build the offline browser comparison from the canonical ROM audit data."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
HTML = r'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>C64 ROM comparison — ReC64 and VICE</title>
<style>
:root { color-scheme: light; --paper: #f5f3eb; --surface: #fffef9; --ink: #20342e; --muted: #5c6c63; --rule: #d5dbce; --accent: #176b57; --tint: #e5eee4; }
* { box-sizing: border-box; }
body { margin: 0; background: var(--paper); color: var(--ink); font: 17px/1.5 Candara, "Trebuchet MS", sans-serif; }
main { max-width: 1540px; margin: auto; padding: 38px 4vw 50px; }
a { color: var(--accent); text-underline-offset: 3px; }
button, input, select { font: inherit; }
button, select { cursor: pointer; }
button, input, select { border: 1px solid var(--rule); background: var(--surface); color: var(--ink); padding: 10px 12px; border-radius: 3px; }
input, select { width: 100%; min-width: 0; }
:focus-visible { outline: 3px solid var(--accent); outline-offset: 3px; }
.eyebrow, .meta, .count, th, .category, .file, .metric-label { font-family: Consolas, "Courier New", monospace; }
.eyebrow { color: var(--accent); font-size: 12px; text-transform: uppercase; letter-spacing: .14em; margin: 0 0 18px; }
header { display: grid; grid-template-columns: 1fr auto; align-items: end; gap: 24px; padding-bottom: 26px; border-bottom: 2px solid var(--ink); }
h1 { font: 400 clamp(34px, 4vw, 60px)/1.08 Georgia, serif; letter-spacing: -.03em; margin: 0 0 14px; }
h2 { font-size: 23px; margin: 0 0 8px; font-weight: 600; }
p { margin: 0 0 10px; }
.intro { color: var(--muted); max-width: 740px; }
.meta { font-size: 12px; color: var(--muted); max-width: 260px; padding-bottom: 8px; }
.meta strong { color: var(--ink); display: block; margin-bottom: 6px; }
.summary { display: grid; grid-template-columns: 1.35fr repeat(3, 1fr); gap: 25px; align-items: center; margin: 28px 0; }
.finding { padding-right: 20px; border-right: 1px solid var(--rule); }
.finding strong { font-size: 20px; display: block; margin-bottom: 6px; }
.finding p, .metric-label { font-size: 14px; line-height: 1.45; color: var(--muted); }
.metric { font: 400 38px/1.15 Georgia, serif; margin-bottom: 8px; }
.metric span { color: var(--muted); font-size: 25px; }
.filters { display: grid; grid-template-columns: 2fr 1.2fr 1.2fr auto; gap: 14px; align-items: end; margin: 24px 0 12px; }
label { display: block; font-size: 14px; font-weight: 600; }
label span { display: block; margin-bottom: 5px; }
.count { font-size: 12px; color: var(--muted); margin: 14px 0; }
.table-wrap { overflow: auto; max-height: 74vh; background: var(--surface); border: 1px solid var(--rule); }
table { border-collapse: collapse; width: 100%; min-width: 910px; }
caption { position: absolute; width: 1px; height: 1px; overflow: hidden; clip-path: inset(50%); }
th { position: sticky; top: 0; z-index: 1; text-align: left; background: var(--tint); font-size: 12px; text-transform: uppercase; letter-spacing: .04em; padding: 14px 17px; border-bottom: 1px solid var(--rule); }
td { vertical-align: top; padding: 17px; border-bottom: 1px solid var(--rule); font-size: 15px; }
tbody tr:last-child td { border-bottom: 0; }
th:nth-child(1) { width: 25%; } th:nth-child(2) { width: 37%; } th:nth-child(3), th:nth-child(4) { width: 19%; }
.name { display: block; font-size: 17px; font-weight: 600; margin-bottom: 5px; }
.category { color: var(--muted); font-size: 11px; }
.supported { color: var(--accent); font-weight: 600; }
.outside { color: var(--muted); }
details { margin-top: 12px; }
summary { color: var(--accent); font-size: 13px; cursor: pointer; width: fit-content; }
.detail { margin-top: 9px; color: var(--muted); font-size: 14px; }
.file { overflow-wrap: anywhere; font-size: 11px; margin-top: 10px; }
.sources { display: flex; flex-direction: column; gap: 3px; margin-top: 10px; font-size: 13px; }
.empty { padding: 32px 0; color: var(--muted); }
footer { display: grid; grid-template-columns: 1.4fr 1fr; gap: 32px; margin-top: 30px; padding-top: 24px; border-top: 1px solid var(--rule); color: var(--muted); font-size: 14px; }
footer h2 { color: var(--ink); font-size: 19px; }
footer a { margin-right: 15px; }
@media (max-width: 900px) { header { grid-template-columns: 1fr; gap: 10px; } .meta { max-width: none; } .summary { grid-template-columns: repeat(3, 1fr); gap: 20px; } .finding { grid-column: 1 / -1; border-right: 0; border-bottom: 1px solid var(--rule); padding-bottom: 12px; } .filters { grid-template-columns: 1fr 1fr; } .search { grid-column: 1 / -1; } .filters button { grid-column: 1 / -1; } footer { grid-template-columns: 1fr; } }
@media (max-width: 520px) { main { padding: 25px 18px; } .summary { gap: 12px; } .metric { font-size: 30px; } .metric span { font-size: 20px; } .metric-label { font: 12px/1.4 Candara, sans-serif; } }
@media print { main { padding: 0; } .filters, .count { display: none; } .table-wrap { overflow: visible; max-height: none; } table { min-width: 0; } th { position: static; } td { padding: 10px; font-size: 11px; } .name { font-size: 13px; } }
</style>
</head>
<body>
<main>
  <p class="eyebrow">ReC64 / compatibility reference</p>
  <header>
    <div><h1>The C64 ROM field guide</h1><p class="intro">What each firmware image and cartridge family does, and where ReC64 and VICE support it.</p></div>
    <div class="meta"><strong id="baseline"></strong><span id="audit-date"></span><br>153 catalogue entries · works offline</div>
  </header>
  <section class="summary" aria-label="Compatibility overview">
    <div class="finding"><strong>Five exact firmware matches</strong><p>All five ReC64 system and drive images match VICE byte for byte. Alternate KERNALs and JiffyDOS currently have no ReC64 loading route.</p></div>
    <div><div class="metric">3 <span>/ 10</span></div><div class="metric-label">Bundled C64 system images recognized by ReC64</div></div>
    <div><div class="metric">2 <span>/ 15</span></div><div class="metric-label">C64 drive firmware roles supported by ReC64</div></div>
    <div><div class="metric">__CART_COUNT__ <span>/ 87</span></div><div class="metric-label">VICE cartridge families supported by ReC64</div></div>
  </section>
  <section aria-label="ROM catalogue">
    <div class="filters">
      <label class="search"><span>Search ROMs and purposes</span><input id="search" type="search" placeholder="1541, freezer, JiffyDOS, Japanese, filename or hash…" autocomplete="off"></label>
      <label><span>Category</span><select id="category"><option value="all">All categories</option></select></label>
      <label><span>ReC64 coverage</span><select id="coverage"><option value="all">All entries</option><option value="yes">Supported / recognized</option><option value="no">Outside current support</option></select></label>
      <button id="reset" type="button">Reset filters</button>
    </div>
    <p class="count" id="count" role="status" aria-live="polite"></p>
    <div class="table-wrap" id="table-wrap" tabindex="0" aria-label="ROM comparison; scroll for more entries">
      <table><caption>C64 firmware and cartridge support in ReC64 and VICE</caption><thead><tr><th scope="col">ROM / device</th><th scope="col">What it is for</th><th scope="col">ReC64</th><th scope="col">VICE</th></tr></thead><tbody id="rows"></tbody></table>
    </div>
    <p class="empty" id="empty" hidden>No entries match these filters. Try a broader search or reset the filters.</p>
  </section>
  <footer>
    <div><h2>Reading the comparison</h2><p>System and drive images use a fixed hash catalogue. Cartridges need a supported hardware mapper and a correctly formed CRT file. A ROM alone cannot add a missing processor, controller or bus.</p><p>The drive total includes three optional CMD ROM slots. Cartridge totals describe hardware families, not individual games. EasyFlash Xbank is excluded. Bundled printer dumps serve virtual printing, rather than full printer CPU emulation. REU and GeoRAM need no ROM.</p></div>
    <div><h2>Sources and scope</h2><p>This is a source and catalogue audit, not a compatibility test of every firmware or game. Expand any entry for details, hashes and its source links.</p><p><a href="C64-ROM-AUDIT.md">Markdown reference</a><a href="C64-ROM-AUDIT.json">Structured data</a><a href="https://github.com/VICE-Team/svn-mirror/releases/tag/r46417" target="_blank" rel="noopener noreferrer">Pinned VICE release</a></p></div>
  </footer>
  <noscript>This searchable comparison needs JavaScript. The Markdown reference above contains the complete inventory.</noscript>
</main>
<script id="audit-data" type="application/json">__AUDIT_DATA__</script>
<script>
"use strict";
const audit = JSON.parse(document.getElementById("audit-data").textContent);
const search = document.getElementById("search"), category = document.getElementById("category"), coverage = document.getElementById("coverage");
const rows = document.getElementById("rows"), wrap = document.getElementById("table-wrap"), empty = document.getElementById("empty");
document.getElementById("baseline").textContent = `ReC64 ${audit.rec64_version} / VICE ${audit.vice_version}`;
document.getElementById("audit-date").textContent = `Support audited ${audit.audit_date}`;
for (const name of new Set(audit.rows.map(row => row.category))) category.add(new Option(name, name));
const searchable = audit.rows.map(row => ({ row, text: [row.name, row.category, row.purpose, row.rec64, row.vice, row.detail, row.file || "", row.sha256 || "", ...(row.aliases || [])].join(" ").toLowerCase() }));
function element(tag, text, className) { const node = document.createElement(tag); if (text !== undefined) node.textContent = text; if (className) node.className = className; return node; }
function render() {
  const words = search.value.toLowerCase().trim().split(/\s+/).filter(Boolean);
  const selected = searchable.filter(({ row, text }) => (category.value === "all" || row.category === category.value) && (coverage.value === "all" || row.rec64_supported === (coverage.value === "yes")) && words.every(word => text.includes(word)));
  const fragment = document.createDocumentFragment();
  for (const { row } of selected) {
    const tr = document.createElement("tr"), name = document.createElement("td"), purpose = document.createElement("td");
    name.append(element("span", row.name, "name"), element("span", row.category, "category"));
    purpose.append(element("span", row.purpose));
    const details = document.createElement("details"); details.append(element("summary", "Details and sources"));
    if (row.detail) details.append(element("p", row.detail, "detail"));
    if (row.file) details.append(element("p", row.file + (row.bytes === undefined ? "" : ` · ${row.bytes.toLocaleString()} bytes`), "file"));
    if (row.sha256) details.append(element("p", `SHA-256: ${row.sha256}`, "file"));
    if (row.aliases && row.aliases.length) details.append(element("p", `Identical files: ${row.aliases.join(", ")}`, "file"));
    const sources = element("div", undefined, "sources");
    for (const key of row.sources) { const source = audit.sources[key], link = element("a", source.title); link.href = source.url; link.target = "_blank"; link.rel = "noopener noreferrer"; sources.append(link); }
    details.append(sources); purpose.append(details);
    tr.append(name, purpose, element("td", row.rec64, row.rec64_supported ? "supported" : "outside"), element("td", row.vice)); fragment.append(tr);
  }
  rows.replaceChildren(fragment); wrap.hidden = selected.length === 0; empty.hidden = selected.length !== 0;
  document.getElementById("count").textContent = `${selected.length} of ${audit.rows.length} entries · select “Details and sources” for image hashes and references`;
  wrap.scrollTop = 0;
}
search.addEventListener("input", render); category.addEventListener("change", render); coverage.addEventListener("change", render);
document.getElementById("reset").addEventListener("click", () => { search.value = ""; category.value = "all"; coverage.value = "all"; render(); search.focus(); });
render();
</script>
</body>
</html>
'''


def main() -> None:
    data = (ROOT / 'docs/C64-ROM-AUDIT.json').read_text(encoding='utf8')
    count = str(json.loads(data)['metrics']['rec64_crt_families'])
    # Keep data inert even if a future description contains HTML-like text.
    data = data.replace('&', r'\u0026').replace('<', r'\u003c').replace('>', r'\u003e')
    target = ROOT / 'docs/C64-ROM-AUDIT.html'
    target.write_text(HTML.replace('__AUDIT_DATA__', data).replace('__CART_COUNT__', count), encoding='utf8', newline='\n')
    print(target)


if __name__ == '__main__':
    main()
