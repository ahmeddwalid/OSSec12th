// Builds the project PDF report from report/report.md.
// Renders Markdown to HTML, expands source-file includes, then prints to PDF
// with Chromium via Puppeteer. Run with: npm run report

import { readFileSync, writeFileSync, mkdirSync, existsSync, rmSync } from "node:fs";
import { dirname, resolve, basename, extname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import MarkdownIt from "markdown-it";
import hljs from "highlight.js";
import puppeteer from "puppeteer";

const __dirname = dirname(fileURLToPath(import.meta.url));
const docsRoot = resolve(__dirname, "..");
const reportDir = resolve(docsRoot, "report");
const staticDir = resolve(docsRoot, "static");
const outDir = resolve(staticDir, "reports");
const outFile = resolve(outDir, "xv6-medical-device-security-report.pdf");

const TEAM = [
  { name: "Ahmed Walid Ibrahim", id: "221011183" },
  { name: "Jana Ashraf Ali", id: "221010291" },
];

const langByExt = { ".c": "c", ".h": "c", ".S": "armasm", ".sh": "bash", ".mk": "makefile" };

function highlight(code, lang) {
  if (lang && hljs.getLanguage(lang)) {
    try {
      return hljs.highlight(code, { language: lang, ignoreIllegals: true }).value;
    } catch {
      /* fall through */
    }
  }
  return hljs.highlightAuto(code).value;
}

// Expand "<!-- INCLUDE path [startLine endLine] -->" markers into highlighted
// listings. We swap them for placeholders so Markdown rendering leaves the
// source untouched, then splice the listing HTML back in afterwards.
function extractIncludes(markdown) {
  const listings = [];
  const re = /<!--\s*INCLUDE\s+(\S+)(?:\s+(\d+)\s+(\d+))?\s*-->/g;
  const text = markdown.replace(re, (_m, relPath, start, end) => {
    const abs = resolve(docsRoot, relPath);
    if (!existsSync(abs)) {
      throw new Error(`INCLUDE target not found: ${relPath} (resolved ${abs})`);
    }
    let src = readFileSync(abs, "utf8").replace(/\s+$/, "");
    let label = basename(abs);
    if (start && end) {
      const lines = src.split("\n").slice(Number(start) - 1, Number(end));
      src = lines.join("\n");
      label = `${basename(abs)} (lines ${start}-${end})`;
    }
    const lang = langByExt[extname(abs)] || "c";
    const html =
      `<p class="listing-caption">Listing: <span class="file">${label}</span></p>` +
      `<pre class="listing"><code class="hljs language-${lang}">${highlight(src, lang)}</code></pre>`;
    const token = `@@LISTING_${listings.length}@@`;
    listings.push(html);
    return token;
  });
  return { text, listings };
}

function buildCover() {
  const people = TEAM.map(
    (m) =>
      `<div class="cover__person"><div class="name">${m.name}</div><div class="id">ID ${m.id}</div></div>`
  ).join("");
  const year = new Date().getFullYear();
  return `
  <section class="cover">
    <div class="cover__inner">
      <span class="cover__badge">CCY4304 &middot; 12th Project &middot; RISC-V xv6</span>
      <h1 class="cover__title">Securing the Kernel,<br/>Protecting Lives</h1>
      <p class="cover__tagline">A medical-device security layer for xv6-riscv: user authentication, file access control, and a syscall audit log.</p>
      <hr class="cover__rule"/>
      <div class="cover__team">${people}</div>
      <div class="cover__meta">
        <div><strong>Course</strong> &nbsp; CCY4304: Operating Systems Security</div>
        <div><strong>Lecturer</strong> &nbsp; Prof. Dr. Ayman Adel Abdel-Hamid</div>
        <div><strong>Teaching Assistant</strong> &nbsp; Abdelrahman Solyman</div>
      </div>
    </div>
    <div class="cover__footer">
      <span>Project Report</span>
      <span>${year}</span>
    </div>
  </section>`;
}

function rewriteImagePaths(html) {
  // Point relative image sources at the real files on disk so Chromium can load
  // them when rendering from setContent (no web server involved).
  return html.replace(/src="(\/)?img\//g, () => `src="${pathToFileURL(resolve(staticDir, "img")).href}/`);
}

async function main() {
  const mdPath = resolve(reportDir, "report.md");
  const cssPath = resolve(reportDir, "report.css");
  const rawMd = readFileSync(mdPath, "utf8");
  const css = readFileSync(cssPath, "utf8");

  const { text, listings } = extractIncludes(rawMd);

  const md = new MarkdownIt({
    html: true,
    linkify: false,
    typographer: false, // keep punctuation literal: no smart quotes, no en/em dash conversion
    highlight,
  });

  let bodyHtml = md.render(text);
  listings.forEach((html, i) => {
    // markdown-it wraps a lone token in a paragraph; replace the whole wrapper.
    const wrapped = new RegExp(`<p>\\s*@@LISTING_${i}@@\\s*</p>`);
    bodyHtml = bodyHtml.replace(wrapped, html).replace(`@@LISTING_${i}@@`, html);
  });
  bodyHtml = rewriteImagePaths(bodyHtml);

  const doc = `<!doctype html>
<html lang="en"><head><meta charset="utf-8"/>
<style>${css}</style></head>
<body>
${buildCover()}
<main class="body">
${bodyHtml}
</main>
</body></html>`;

  mkdirSync(outDir, { recursive: true });

  // Write to a temp HTML file and load it with a file:// origin. Loading via
  // setContent leaves the page on about:blank, and Chromium then refuses to
  // load the local screenshot files as subresources.
  const tmpHtml = resolve(outDir, ".report.tmp.html");
  writeFileSync(tmpHtml, doc, "utf8");

  const browser = await puppeteer.launch({
    headless: true,
    args: ["--no-sandbox", "--disable-setuid-sandbox", "--font-render-hinting=none"],
  });
  try {
    const page = await browser.newPage();
    await page.goto(pathToFileURL(tmpHtml).href, { waitUntil: "networkidle0" });
    await page.pdf({
      path: outFile,
      format: "A4",
      printBackground: true,
      displayHeaderFooter: true,
      headerTemplate: "<span></span>",
      footerTemplate:
        '<div style="width:100%;font-size:7pt;color:#8a98a5;padding:0 16mm;font-family:Arial,sans-serif;display:flex;justify-content:space-between;">' +
        "<span>xv6 Medical Device Security</span>" +
        '<span>Page <span class="pageNumber"></span> of <span class="totalPages"></span></span>' +
        "</div>",
      margin: { top: "18mm", bottom: "20mm", left: "16mm", right: "16mm" },
    });
  } finally {
    await browser.close();
    rmSync(tmpHtml, { force: true });
  }

  console.log(`Report written to ${outFile}`);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
