import type { SidebarsConfig } from '@docusaurus/plugin-content-docs';

const sidebars: SidebarsConfig = {
  mainSidebar: [
    'intro',
    'environment-setup',
    {
      type: 'category',
      label: '🔐 Security Phases',
      collapsed: false,
      items: [
        'phase1-authentication',
        'phase2-file-permissions',
        'phase3-audit-log',
      ],
    },
    {
      type: 'category',
      label: '🧪 Testing & Compliance',
      collapsed: false,
      items: ['bonus-compliance-testing'],
    },
    {
      type: 'category',
      label: '📋 Standards & Context',
      collapsed: false,
      items: ['fda-iec62443-context'],
    },
  ],
};

export default sidebars;
