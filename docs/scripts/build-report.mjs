// Builds the project PDF report from report/report.md.
// Workflow: parse md → expand INCLUDE directives → render HTML → print to PDF.
// Two-pass: first pass measures section page numbers, second pass fills the TOC.

import { readFileSync, writeFileSync, mkdirSync, existsSync, rmSync } from "node:fs";
import { dirname, resolve, basename, extname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { createRequire } from "node:module";
import MarkdownIt from "markdown-it";
import hljs from "highlight.js";
import puppeteer from "puppeteer";

const require = createRequire(import.meta.url);
// Require the lib directly: pdf-parse's index.js has a debug block that reads a
// bundled test PDF when module.parent is unset, which throws under ESM.
const pdfParse = require("pdf-parse/lib/pdf-parse.js");

const __dirname = dirname(fileURLToPath(import.meta.url));
const docsRoot = resolve(__dirname, "..");
const reportDir = resolve(docsRoot, "report");
const staticDir = resolve(docsRoot, "static");
const outDir = resolve(staticDir, "reports");
const outFile = resolve(outDir, "xv6-medical-device-security-report.pdf");
const tmpPdf = resolve(outDir, ".report.pass1.pdf");
const logoUrl = pathToFileURL(resolve(staticDir, "img/logos/AAST Logo.png")).href;

const TEAM = [
  { name: "Ahmed Walid Ibrahim", id: "221011183" },
  { name: "Jana Ashraf Ali", id: "221010291" },
];

// Top-level report sections, in document order. `title` must match the H1 text
// in report.md; `key` is a distinctive fallback used if extraction drops the
// colon or punctuation. Used to look up each section's page for the contents.
const SECTIONS = [
  { title: "Abstract", key: "Abstract" },
  { title: "Environment and Build", key: "Environment and Build" },
  { title: "Phase 1: User Authentication", key: "User Authentication" },
  { title: "Phase 2: File Access Control", key: "File Access Control" },
  { title: "Phase 3: Syscall Audit Log", key: "Syscall Audit Log" },
  { title: "Compliance Testing", key: "Compliance Testing" },
  { title: "Regulatory Context", key: "Regulatory Context" },
  { title: "Appendix A: Source Code", key: "Appendix A" },
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

// Expand "<!-- INCLUDE rel_path [start_line end_line] -->" directives into
// syntax-highlighted code listings. We swap them for @@LISTING_N@@ tokens so
// markdown-it doesn't mangle the code, then splice the rendered HTML back in.
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
    <div class="cover__logo"><img src="${logoUrl}" alt="Arab Academy for Science, Technology and Maritime Transport"/></div>
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

function buildToc(pageMap) {
  const rows = SECTIONS.map((s) => {
    const num = pageMap ? pageMap.get(s.title) ?? "" : "";
    return (
      `<div class="toc__row"><span class="toc__title">${s.title}</span>` +
      `<span class="toc__dots"></span>` +
      `<span class="toc__page">${num || "&middot;&middot;"}</span></div>`
    );
  }).join("");
  return `
  <section class="toc">
    <h1 class="toc__heading">Table of Contents</h1>
    ${rows}
  </section>`;
}

// Measure which page each section starts on by extracting text from every page
// of a pass-1 PDF. pdf-parse's pagerender callback returns text content.
async function measurePages(pdfBuffer, firstContentPage) {
  const pages = [];
  await pdfParse(pdfBuffer, {
    pagerender: (pageData) =>
      pageData.getTextContent().then((tc) => {
        pages.push(tc.items.map((i) => i.str).join(" "));
        return "";
      }),
  });
  const norm = (s) => s.replace(/\s+/g, " ").trim();
  const map = new Map();
  for (const section of SECTIONS) {
    const title = norm(section.title);
    const key = norm(section.key);
    for (let p = firstContentPage - 1; p < pages.length; p++) {
      const text = norm(pages[p]);
      if (text.includes(title) || text.includes(key)) {
        map.set(section.title, p + 1); // 1-based page number
        break;
      }
    }
  }
  return map;
}

function rewriteImagePaths(html) {
  // Chromium won't load relative image references from setContent(); rewrite
  // /img/... paths to file:// urls pointing at static/img/.
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

  // The cover is page 1 and the contents page is page 2, so body content starts
  // on page 3.
  const FIRST_CONTENT_PAGE = 3;
  const compose = (tocHtml) =>
    `<!doctype html>
<html lang="en"><head><meta charset="utf-8"/>
<style>${css}</style></head>
<body>
${buildCover()}
${tocHtml}
<main class="body">
${bodyHtml}
</main>
</body></html>`;

  mkdirSync(outDir, { recursive: true });

  const browser = await puppeteer.launch({
    headless: true,
    // --no-sandbox needed for CI/container environments where chrome runs as root
    args: ["--no-sandbox", "--disable-setuid-sandbox", "--font-render-hinting=none"],
  });

  // Load HTML from a temp file so the page has a file:// origin; Chromium refuses
  // to load the local screenshots as subresources from an about:blank document.
  const renderPdf = async (doc, outPath) => {
    const tmpHtml = resolve(outDir, ".report.tmp.html");
    writeFileSync(tmpHtml, doc, "utf8");
    const page = await browser.newPage();
    try {
      await page.goto(pathToFileURL(tmpHtml).href, { waitUntil: "networkidle0" });
      await page.pdf({
        path: outPath,
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
      await page.close();
      rmSync(tmpHtml, { force: true });
    }
  };

  try {
  // Pass 1: render with empty TOC → measure section pages → build real TOC.
  // The TOC page size stays the same between passes, so measured numbers are valid.
    await renderPdf(compose(buildToc(null)), tmpPdf);
    const pageMap = await measurePages(readFileSync(tmpPdf), FIRST_CONTENT_PAGE);

    // Pass 2: render the final PDF with real page numbers in the contents.
    await renderPdf(compose(buildToc(pageMap)), outFile);
  } finally {
    await browser.close();
    rmSync(tmpPdf, { force: true });
  }

  console.log(`Report written to ${outFile}`);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
