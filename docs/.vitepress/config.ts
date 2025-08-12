import { defineConfig } from 'vitepress'

// https://vitepress.dev/reference/site-config
export default defineConfig({
  title: "clice",
  description: "a powerful and modern C++ language server",
  base: '/',
  rewrites: {
    'en/:rest*': ':rest*',
  },
  locales: {
    root: { label: 'English' },
    zh: { label: '简体中文' },
  },
  themeConfig: {
    // https://vitepress.dev/reference/default-theme-config
    nav: [
      { text: 'Home', link: '/' },
      { text: 'Examples', link: '/markdown-examples' }
    ],

    sidebar: [
      {
        text: 'Guide',
        items: [
          { text: 'Guide', link: '/guide' },
          { text: 'Runtime API Examples', link: '/api-examples' }
        ]
      }
    ],

    socialLinks: [
      { icon: 'github', link: 'https://github.com/clice-project/clice' }
    ],

    outline: 'deep', 
  }
})
