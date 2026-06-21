/** @type {import('jest').Config} */
module.exports = {
  testEnvironment: "node",
  roots: ["<rootDir>/src"],
  testMatch: ["**/*.test.ts"],
  // Mirror the tsconfig baseUrl="src" alias used by non-relative imports.
  moduleNameMapper: {
    "^config$": "<rootDir>/src/config.ts",
  },
  transform: {
    "^.+\\.ts$": [
      "ts-jest",
      {
        tsconfig: {
          module: "CommonJS",
          moduleResolution: "node",
          verbatimModuleSyntax: false,
          isolatedModules: true,
        },
      },
    ],
  },
};
