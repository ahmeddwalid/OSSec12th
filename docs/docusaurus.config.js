// @ts-check

const config = {
  title: 'xv6 Medical Device Security',
  tagline: 'CCY4304 12th Project documentation',
  url: 'https://ahmeddwalid.github.io',
  baseUrl: '/OSSec12th/',
  organizationName: 'ahmeddwalid',
  projectName: 'OSSec12th',
  onBrokenLinks: 'throw',
  markdown: {
    hooks: {
      onBrokenMarkdownLinks: 'warn',
    },
  },
  i18n: {
    defaultLocale: 'en',
    locales: ['en'],
  },
  presets: [
    [
      'classic',
      {
        docs: {
          sidebarPath: './sidebars.js',
          routeBasePath: '/',
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      },
    ],
  ],
  themeConfig: {
    navbar: {
      title: 'xv6 Medical Device Security',
      items: [
        {
          type: 'docSidebar',
          sidebarId: 'projectSidebar',
          position: 'left',
          label: 'Project Docs',
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
          title: 'Project',
          items: [
            { label: 'Overview', to: '/' },
            { label: 'Compliance Testing', to: '/bonus-compliance-testing' },
          ],
        },
        {
          title: 'Source',
          items: [
            { label: 'GitHub Repository', href: 'https://github.com/ahmeddwalid/OSSec12th' },
          ],
        },
      ],
      copyright: `CCY4304 12th Project - Ahmed Walid - 221011183`,
    },
    prism: {
      additionalLanguages: ['bash', 'c'],
    },
  },
};

module.exports = config;
