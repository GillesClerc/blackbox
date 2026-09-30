import { fileURLToPath } from "node:url";
import { defineConfig } from "vitest/config";

// Alias « @/ » du tsconfig, pour tester le code applicatif tel qu'il est importé.
export default defineConfig({
  resolve: {
    alias: { "@": fileURLToPath(new URL(".", import.meta.url)) },
  },
  test: {
    include: ["**/*.test.ts"],
    exclude: ["node_modules/**", ".next/**"],
  },
});
