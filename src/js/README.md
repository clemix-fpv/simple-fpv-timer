# Build the app

```
esbuild src/app.ts --bundle --outfile=../src/app.js --minify --watch
```
esbuild src/app.ts --bundle --outfile=../src/app.js --minify --watch --target=esnext --sourcemap
