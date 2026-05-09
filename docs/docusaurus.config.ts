import type { Config } from '@docusaurus/types';
import type * as Preset from '@docusaurus/preset-classic';

const config: Config = {
  title: 'xv6 Medical Device Security',
  tagline: 'Securing the Kernel. Protecting Lives.',
  url: 'https://ossec.ahmeddwalid.me',
  baseUrl: '/',
  organizationName: 'ahmeddwalid',
  projectName: 'OSSec12th',
  onBrokenLinks: 'throw',
  trailingSlash: false,
  markdown: {
    mermaid: true,
    hooks: {
      onBrokenMarkdownLinks: 'warn',
    },
  },
  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },
  themes: ['@docusaurus/theme-mermaid'],
  plugins: [
    [
      require.resolve('@easyops-cn/docusaurus-search-local'),
      {
        hashed: true,
        language: ['en'],
        docsRouteBasePath: '/docs',
        indexDocs: true,
        indexPages: true,
        searchBarShortcutHint: false,
      },
    ],
  ],
  presets: [
    [
      'classic',
      {
        docs: {
          sidebarPath: './sidebars.ts',
          routeBasePath: '/docs',
          editUrl: 'https://github.com/ahmeddwalid/OSSec12th/edit/main/docs/',
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      } satisfies Preset.Options,
    ],
  ],
  themeConfig: {
    colorMode: {
      defaultMode: 'dark',
      disableSwitch: false,
      respectPrefersColorScheme: false,
    },
    mermaid: {
      theme: { light: 'neutral', dark: 'dark' },
    },
    image: 'img/og-card.png',
    navbar: {
      title: 'xv6 Med-Sec',
      items: [
        {
          type: 'docSidebar',
          sidebarId: 'mainSidebar',
          position: 'left',
          label: 'Docs',
        },
        {
          to: '/docs/bonus-compliance-testing',
          label: 'Compliance',
          position: 'left',
        },
        {
          href: 'https://github.com/ahmeddwalid/OSSec12th',
          label: 'GitHub',
          position: 'right',
        },
      ],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: 'Documentation',
          items: [
            { label: 'Overview', to: '/docs/intro' },
            { label: 'Environment Setup', to: '/docs/environment-setup' },
            { label: 'Phase 1: Authentication', to: '/docs/phase1-authentication' },
            { label: 'Phase 2: File Permissions', to: '/docs/phase2-file-permissions' },
            { label: 'Phase 3: Audit Log', to: '/docs/phase3-audit-log' },
            { label: 'Compliance Testing', to: '/docs/bonus-compliance-testing' },
            { label: 'FDA / IEC 62443 Context', to: '/docs/fda-iec62443-context' },
          ],
        },
        {
          title: 'Source',
          items: [
            {
              label: 'GitHub Repository',
              href: 'https://github.com/ahmeddwalid/OSSec12th',
            },
          ],
        },
        {
          title: 'Course',
          items: [
            {
              label: 'CCY4304: Operating Systems Security',
              href: 'https://github.com/ahmeddwalid/OSSec12th',
            },
          ],
        },
      ],
      copyright: `
        <div style="line-height:1.8;font-size:0.85rem;opacity:0.85">
          <strong>CCY4304: 12th Project: xv6 Medical Device Security</strong><br/>
          Ahmed Walid Ibrahim (221011183) &amp; Ahmed Mohamed Mahmoud (221010720)<br/>
          Lecturer: Prof. Dr. Ayman Adel Abdel-Hamid &nbsp;|&nbsp; TA: Abdelrahman Solyman<br/>
          Built with Docusaurus ${new Date().getFullYear()}
        </div>
      `,
    },
    prism: {
      theme: require('prism-react-renderer').themes.github,
      darkTheme: require('prism-react-renderer').themes.vsDark,
      additionalLanguages: ['bash', 'c', 'makefile', 'perl'],
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
