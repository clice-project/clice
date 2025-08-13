import fs from 'fs'
import path from 'path'
import { DefaultTheme } from 'vitepress'

export const genSidebar = (
    dirPath: string,
    options: {
        title: string
        collapsible?: boolean
        ignore?: string[]
    }
): DefaultTheme.SidebarItem => {
    const sidebarPath = path.resolve(process.cwd(), dirPath)
    const ignore = options.ignore || ['index.md']

    const files = fs
        .readdirSync(sidebarPath)
        .filter((file) => file.endsWith('.md') && !ignore.includes(file))

    const items = files.map((file) => {
        const content = fs.readFileSync(path.resolve(sidebarPath, file), 'utf-8')
        const match = content.match(/^#\s+(.*)/)
        const title = match ? match[1] : file.replace('.md', '')

        return {
            text: title,
            link: `/${dirPath}/${file.replace('.md', '')}`
        }
    })

    return {
        text: options.title,
        collapsed: options.collapsible || false,
        items
    }
}