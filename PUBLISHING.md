# Publishing the npm package

Publish the `@savoirfairelinux/jami-core@0.4.1` native module packages first.

For each supported platform, build `jamid.node`, copy it into the matching platform package, and publish that platform package:

```sh
npm run pack-native --workspace @savoirfairelinux/jami-core
npm publish daemon/bin/nodejs/npm/$(node -e "process.stdout.write(process.platform + '-' + process.arch)") --access public
```

Then publish the core wrapper:

```sh
npm publish --workspace @savoirfairelinux/jami-core
```

From the repository root, build and inspect the package tarball with:

```sh
npm run npm:pack
```

Publish it with:

```sh
npm run npm:publish
```

The published package is produced from the `server` workspace and includes the compiled server plus the built React client.
