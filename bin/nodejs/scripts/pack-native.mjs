#!/usr/bin/env node
/*
 * Copyright (C) 2025 Savoir-faire Linux Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public
 * License along with this program.  If not, see
 * <https://www.gnu.org/licenses/>.
 */

/**
 * Copy the built jamid.node into the platform-specific npm package directory.
 *
 * Usage:
 *   node scripts/pack-native.mjs [path/to/jamid.node]
 *
 * If no path is given, it looks at ../daemon/bin/nodejs/build/Release/jamid.node.
 */

import { copyFileSync, existsSync } from 'node:fs'
import { dirname, join, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

const __dirname = dirname(fileURLToPath(import.meta.url))
const coreDir = resolve(__dirname, '..')

const platform = process.platform
const arch = process.arch
const targetDir = join(coreDir, 'npm', `${platform}-${arch}`)

if (!existsSync(join(targetDir, 'package.json'))) {
  console.error(`No platform package found at ${targetDir}`)
  console.error(`Supported: darwin-arm64, linux-x64, linux-arm64`)
  process.exit(1)
}

const defaultSource = resolve(coreDir, 'build', 'Release', 'jamid.node')
const source = process.argv[2] ? resolve(process.argv[2]) : defaultSource

if (!existsSync(source)) {
  console.error(`jamid.node not found at ${source}`)
  console.error(`Build the daemon with -DJAMI_NODEJS=ON first, or pass the path as argument.`)
  process.exit(1)
}

const dest = join(targetDir, 'jamid.node')
copyFileSync(source, dest)
console.log(`Copied ${source} -> ${dest}`)
