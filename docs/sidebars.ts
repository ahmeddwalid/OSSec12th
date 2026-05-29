import type { SidebarsConfig } from '@docusaurus/plugin-content-docs';

// sidebar groups docs into logical sections: overview → phases → testing → context
const sidebars: SidebarsConfig = {
  mainSidebar: [
    'intro',
    'environment-setup',
    {
      type: 'category',
      label: 'Security Phases',
      collapsed: false,
      items: [
        'phase1-authentication',
        'phase2-file-permissions',
        'phase3-audit-log',
      ],
    },
    {
      type: 'category',
      label: 'Testing and Compliance',
      collapsed: false,
      items: ['bonus-compliance-testing'],
    },
    {
      type: 'category',
      label: 'Standards and Context',
      collapsed: false,
      items: ['fda-iec62443-context'],
    },
  ],
};

export default sidebars;
