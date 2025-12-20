/** @type {import('tailwindcss').Config} */
module.exports = {
    content: ["./index.html"],
    theme: {
        extend: {
            fontFamily: {sans: ['Inter', 'sans-serif']},
            colors: {
                haiku: {50: '#EEF2FF', 100: '#E0E7FF', 500: '#6366F1', 900: '#312E81'},
                sidebar: '#18181b', sidebarBorder: '#27272a'
            }
        },
    },
    plugins: [],
}
