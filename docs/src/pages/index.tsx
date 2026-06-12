import React from 'react';
import Link from '@docusaurus/Link';
import useDocusaurusContext from '@docusaurus/useDocusaurusContext';
import Layout from '@theme/Layout';
import clsx from 'clsx';
import styles from './index.module.css';

/* ─── stat chips: key numbers from the project ─── */
const STATS = [
  { value: '18/18', label: 'Compliance Tests Passed' },
  { value: '3', label: 'Security Phases' },
  { value: '∞', label: 'Patient Lives Protected' },
];

/* ─── phase cards: one per security phase, each with a code snippet teaser ─── */
const PHASES = [
  {
    icon: 'Auth',
    title: 'Phase 1: Authentication',
    description:
      'UID/GID identity added to every xv6 process. SHA-256 credential lookup from /etc/passwd. Boot-time login enforces identity before any shell access.',
    snippet: `struct proc {
  uint uid;   /* user id     */
  uint gid;   /* group id    */
  uint role;  /* 0=user 1=admin */
  char uname[MAXNAME];
  int  authenticated;
};`,
    to: '/docs/phase1-authentication',
  },
  {
    icon: 'DAC',
    title: 'Phase 2: File Permissions',
    description:
      'Unix rwxrwxrwx mode bits + owner UID/GID on every inode. Four kernel hook points enforce DAC before any read, write, exec, or stat operation.',
    snippet: `struct dinode {
  /* ... existing fields ... */
  uint mode;  /* rwxrwxrwx bits */
  uint uid;   /* owner user id  */
  uint gid;   /* owner group id */
};`,
    to: '/docs/phase2-file-permissions',
  },
  {
    icon: 'Audit',
    title: 'Phase 3: Audit Log',
    description:
      'A 256-entry kernel ring buffer records every security event with timestamp, UID, syscall, and result. Admin-only audit_read syscall; non-admins receive EPERM.',
    snippet: `struct audit_entry {
  uint   timestamp;
  uint   uid;
  int    syscall_num;
  char   filename[MAXPATH];
  int    result;      /* 0=allow -1=deny */
};`,
    to: '/docs/phase3-audit-log',
  },
];

/* ─── arch layers: bottom-up view of the security stack. top layer runs first;
   lower layers are called via ecall ─── */
const ARCH_LAYERS = [
  { color: '#dc2626', label: 'compliance_test  |  login.c  |  user programs' },
  { color: '#d97706', label: 'usys.S syscall stubs  →  ECALL trap' },
  { color: '#16a34a', label: 'Phase 1: auth_check()   Phase 2: perm_check()   Phase 3: audit_log()' },
  { color: '#2563eb', label: 'Modified kernel: proc.c  sysfile.c  fs.c  trap.c' },
  { color: '#7c3aed', label: 'xv6-riscv hardware (QEMU virt board)' },
];

/* ─── terminal simulation: raw output from compliance_test in a fake macos
   terminal window. each line is pre-marked PASS/FAIL for styling ─── */
const COMPLIANCE_LINES = [
  'T01: boot as root (admin)                   PASS',
  'T02: uid=0 after root login                 PASS',
  'T03: bad password rejected                  PASS',
  'T04: good password accepted                 PASS',
  'T05: file owner set on create               PASS',
  'T06: world-readable file readable           PASS',
  'T07: chmod 000 blocks owner                 PASS',
  'T08: chmod 644 allows owner read            PASS',
  'T09: chown changes owner                    PASS',
  'T10: admin can read any file                PASS',
  'T11: user cannot read root file             PASS',
  'T12: audit_read requires admin              PASS',
  'T13: audit log non-empty after ops          PASS',
  'T14: audit entries contain uid              PASS',
  'T15: exec blocked by perm                   PASS',
  'T16: write blocked by perm                  PASS',
  'T17: end-to-end medical workflow            PASS',
  'T18: role escalation prevented              PASS',
  '',
  'Result: 18 / 18 tests passed',
];

/* ─── team members ─── */
const TEAM = [
  { name: 'Ahmed Walid Ibrahim', id: '221011183', linkedin: 'https://www.linkedin.com/in/ahmeddwalid/' },
  { name: 'Jana Ashraf Ali', id: '221010291', linkedin: 'https://www.linkedin.com/in/janaaashraf/' },
];

/* ════════════════════════════════════════════════════════════
   Hero — oversized headline, stat chips, cta buttons
   ════════════════════════════════════════════════════════════ */
function Hero(): React.ReactElement {
  return (
    <header className={styles.hero}>
      <div className={styles.heroInner}>
        <span className={styles.badge}>CCY4304 · 12th Project · RISC-V xv6</span>
        <h1 className={styles.heroTitle}>
          Securing the Kernel.<br />Protecting Lives.
        </h1>
        <p className={styles.heroSubtitle}>
          A medical-device OS security layer built on xv6-riscv: authentication,
          discretionary access control, and syscall audit logging, designed to the
          spirit of FDA 2023 guidance and IEC 62443.
        </p>
        <div className={styles.heroButtons}>
          <Link className={clsx('button button--primary button--lg', styles.ctaPrimary)} to="/docs/intro">
            Read the Docs
          </Link>
          <Link
            className={clsx('button button--outline button--lg', styles.ctaSecondary)}
            href="https://github.com/ahmeddwalid/OSSec12th"
          >
            View on GitHub
          </Link>
        </div>
        <div className={styles.statsRow}>
          {STATS.map((s) => (
            <div key={s.label} className={styles.statChip}>
              <span className={styles.statValue}>{s.value}</span>
              <span className={styles.statLabel}>{s.label}</span>
            </div>
          ))}
        </div>
      </div>
    </header>
  );
}

/* ════════════════════════════════════════════════════════════
   WhatIsThis — medtronic story, why xv6, project justification
   ════════════════════════════════════════════════════════════ */
function WhatIsThis(): React.ReactElement {
  return (
    <section className={styles.section}>
      <div className={clsx(styles.container, styles.twoCol)}>
        <div>
          <h2 className={styles.sectionTitle}>What is this project?</h2>
          <p>
            In 2019 Medtronic disclosed that its MiniMed 508 insulin pump could be
            wirelessly commanded to deliver a lethal overdose, with no authentication
            required. The root cause was a complete absence of OS-level access
            controls.
          </p>
          <p>
            This project retrofits <strong>xv6-riscv</strong>, MIT's teaching
            kernel used in OS courses worldwide, with three security layers that
            mirror the controls a real medical-device OS must provide:
          </p>
          <ul>
            <li>User identity and boot-time authentication</li>
            <li>Inode-level discretionary access control (DAC)</li>
            <li>Immutable kernel audit trail for every security decision</li>
          </ul>
          <Link className="button button--outline button--sm" to="/docs/fda-iec62443-context">
            Read the regulatory context →
          </Link>
        </div>
        <div className={styles.xv6Box}>
          <h3>Why xv6?</h3>
          <p>
            xv6 is intentionally minimal: ~10 000 lines of C, no MMU complexity,
            no driver jungle. Every security hook you add is immediately visible in
            context. That makes it the perfect teaching substrate for medical-device
            OS concepts where every line of kernel code must be auditable.
          </p>
          <p>
            The kernel runs on <code>qemu-system-riscv64</code> (virt board). The
            RISC-V ISA's privilege levels (U/S/M) map cleanly to xv6's user/kernel
            separation.
          </p>
        </div>
      </div>
    </section>
  );
}

/* ════════════════════════════════════════════════════════════
   Three Phases
════════════════════════════════════════════════════════════ */
function Phases(): React.ReactElement {
  return (
    <section className={clsx(styles.section, styles.sectionAlt)}>
      <div className={styles.container}>
        <h2 className={clsx(styles.sectionTitle, styles.centered)}>Three Security Phases</h2>
        <p className={clsx(styles.sectionSubtitle, styles.centered)}>
          Each phase is a self-contained kernel modification. Together they satisfy
          the CIA triad for a medical-device OS.
        </p>
        <div className={styles.phaseGrid}>
          {PHASES.map((p) => (
            <div key={p.title} className={styles.phaseCard}>
              <div className={styles.phaseIcon}>{p.icon}</div>
              <h3 className={styles.phaseTitle}>{p.title}</h3>
              <p className={styles.phaseDesc}>{p.description}</p>
              {/* inline code snippet gives a preview of the data structure changes */}
              <pre className={styles.codeSnippet}><code>{p.snippet}</code></pre>
              <Link className="button button--sm button--outline" to={p.to}>
                View implementation →
              </Link>
            </div>
          ))}
        </div>
      </div>
    </section>
  );
}

/* ════════════════════════════════════════════════════════════
   Architecture
════════════════════════════════════════════════════════════ */
function Architecture(): React.ReactElement {
  return (
    <section className={styles.section}>
      <div className={styles.container}>
        <h2 className={clsx(styles.sectionTitle, styles.centered)}>Architecture Overview</h2>
        <p className={clsx(styles.sectionSubtitle, styles.centered)}>
          Security hooks intercept every syscall before kernel resources are touched.
        </p>
        <div className={styles.archDiagram}>
          {ARCH_LAYERS.map((l, i) => (
            <div
              key={i}
              className={styles.archLayer}
              // color-coded layer: red(user) → orange(syscall) → green(hooks) → blue(kernel) → purple(hw)
              style={{ borderLeftColor: l.color, background: `${l.color}18` }}
            >
              <span className={styles.archLayerNum}>{ARCH_LAYERS.length - i}</span>
              <span className={styles.archLayerLabel}>{l.label}</span>
            </div>
          ))}
          <div className={styles.archArrow}>↑ privilege escalation direction (ECALL)</div>
        </div>
      </div>
    </section>
  );
}

/* ════════════════════════════════════════════════════════════
   Compliance Report
════════════════════════════════════════════════════════════ */
function ComplianceReport(): React.ReactElement {
  return (
    <section className={clsx(styles.section, styles.sectionAlt)}>
      <div className={styles.container}>
        <h2 className={clsx(styles.sectionTitle, styles.centered)}>Compliance Report</h2>
        <p className={clsx(styles.sectionSubtitle, styles.centered)}>
          18 automated tests run on real xv6 inside QEMU. All pass.
        </p>
        <div className={styles.terminal}>
          <div className={styles.terminalBar}>
            <span className={styles.termDot} style={{ background: '#ff5f57' }} />
            <span className={styles.termDot} style={{ background: '#febc2e' }} />
            <span className={styles.termDot} style={{ background: '#28c840' }} />
            <span className={styles.terminalTitle}>compliance_test output</span>
          </div>
          <pre className={styles.terminalBody}>
            {COMPLIANCE_LINES.map((line, i) => (
              <div key={i} className={line.includes('PASS') ? styles.passLine : styles.normalLine}>
                {line}
              </div>
            ))}
          </pre>
        </div>
        <div className={styles.centeredBtn}>
          <Link className="button button--primary" to="/docs/bonus-compliance-testing">
            Read the full test breakdown →
          </Link>
        </div>
      </div>
    </section>
  );
}

/* ════════════════════════════════════════════════════════════
   Team
════════════════════════════════════════════════════════════ */
function Team(): React.ReactElement {
  return (
    <section className={styles.section}>
      <div className={styles.container}>
        <h2 className={clsx(styles.sectionTitle, styles.centered)}>Team & Course</h2>
        <div className={styles.teamGrid}>
          {TEAM.map((m) => (
            <a key={m.id} href={m.linkedin} target="_blank" rel="noreferrer" className={styles.teamCard}>

              <div className={styles.teamName}>{m.name}</div>
              <div className={styles.teamId}>ID: {m.id}</div>
              <span className={styles.linkedinBadge}>
                <svg width="14" height="14" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true">
                  <path d="M20.447 20.452h-3.554v-5.569c0-1.328-.027-3.037-1.852-3.037-1.853 0-2.136 1.445-2.136 2.939v5.667H9.351V9h3.414v1.561h.046c.477-.9 1.637-1.85 3.37-1.85 3.601 0 4.267 2.37 4.267 5.455v6.286zM5.337 7.433a2.062 2.062 0 0 1-2.063-2.065 2.064 2.064 0 1 1 2.063 2.065zm1.782 13.019H3.555V9h3.564v11.452zM22.225 0H1.771C.792 0 0 .774 0 1.729v20.542C0 23.227.792 24 1.771 24h20.451C23.2 24 24 23.227 24 22.271V1.729C24 .774 23.2 0 22.222 0h.003z"/>
                </svg>
                LinkedIn
              </span>
            </a>
          ))}
        </div>
        <div className={styles.courseInfo}>
          <p><strong>Course:</strong> CCY4304: Operating Systems Security</p>
          <p><strong>Lecturer:</strong> Prof. Dr. Ayman Adel Abdel-Hamid</p>
          <p><strong>Teaching Assistant:</strong> Abdelrahman Solyman</p>
        </div>
      </div>
    </section>
  );
}

/* ════════════════════════════════════════════════════════════
   Footer CTA
════════════════════════════════════════════════════════════ */
function FooterCTA(): React.ReactElement {
  return (
    <section className={styles.footerCTA}>
      <div className={styles.container}>
        <h2>Ready to explore the kernel code?</h2>
        <p>Full source, documentation, and CI pipeline on GitHub.</p>
        <div className={styles.heroButtons}>
          <Link className="button button--primary button--lg" to="/docs/intro">
            Start reading →
          </Link>
          <Link
            className="button button--outline button--lg"
            href="https://github.com/ahmeddwalid/OSSec12th"
          >
            Browse source
          </Link>
        </div>
      </div>
    </section>
  );
}

/* ════════════════════════════════════════════════════════════
   Page root
════════════════════════════════════════════════════════════ */
export default function Home(): React.ReactElement {
  const { siteConfig } = useDocusaurusContext();
  return (
    <Layout
      title={siteConfig.title}
      description="CCY4304 12th Project: xv6 medical device OS security: authentication, DAC file permissions, and syscall audit logging on RISC-V."
    >
      <Hero />
      <main>
        <WhatIsThis />
        <Phases />
        <Architecture />
        <ComplianceReport />
        <Team />
        <FooterCTA />
      </main>
    </Layout>
  );
}
