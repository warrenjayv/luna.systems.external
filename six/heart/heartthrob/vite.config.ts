import { defineConfig } from "vite";

// https://vite.dev/config/
export default defineConfig({
  server: {
    port: 8080,
    open: false,
  },
  optimizeDeps: {
    exclude: ['koffi']
  },
  build: {
    rollupOptions: {
      external: ['koffi']
    }
  }
});
