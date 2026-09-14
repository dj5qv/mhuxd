import { defineConfig } from 'vite';
import { svelte } from '@sveltejs/vite-plugin-svelte';

export default defineConfig({
  base: '/svelte/',
  plugins: [svelte()],
  server: {
    port: 5173,
    strictPort: true
  },
  build: {
      minify: false,
      rollupOptions: {
        output: {
          entryFileNames: `assets/[name].js`,
          chunkFileNames: `assets/[name].js`,
          assetFileNames: `assets/[name].[ext]`
        }
      }
    }  
});
