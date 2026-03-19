# react-native-nitro-archive

High-performance native archive module for React Native, powered by [Nitro Modules](https://nitro.margelo.com/). Supports **tar**, **tar.bz2**, **tar.gz**, **bz2**, **gz**, **zip** (with AES-256 encryption), and **7z** (unpack only) — all via synchronous C++ streaming I/O with no JS bridge overhead.

## Supported Formats

| Format | Pack | Unpack | List | Detect | Test |
|--------|------|--------|------|--------|------|
| `tar` | Yes | Yes | Yes | Yes | Yes |
| `tar.bz2` | Yes | Yes | Yes | Yes | Yes |
| `tar.gz` | Yes | Yes | Yes | Yes | Yes |
| `bz2` | Yes | Yes | — | Yes | — |
| `gz` | Yes | Yes | — | Yes | — |
| `zip` | Yes | Yes | Yes | Yes | Yes |
| `sevenz` (7z) | — | Yes | Yes | Yes | Yes |

## Installation

```sh
npm install react-native-nitro-archive react-native-nitro-modules
```

### Expo

The package ships with an Expo config plugin:

```json
{
  "plugins": ["react-native-nitro-archive"]
}
```

Then run `npx expo prebuild`.

### iOS

```sh
cd ios && pod install
```

### Android

No additional steps — autolinking handles everything.

## Usage

```typescript
import {
  pack,
  unpack,
  listContents,
  detectFormat,
  testArchive,
} from 'react-native-nitro-archive';
```

### Pack (compress)

```typescript
const result = await pack('/path/to/source', '/path/to/output.tar.bz2', 'tar.bz2');

if (result.success) {
  console.log(`Packed ${result.bytesProcessed} bytes to ${result.outputPath}`);
}
```

### Unpack (decompress)

```typescript
const result = await unpack('/path/to/archive.zip', '/path/to/output', true);
// third argument: overwrite existing files (true/false)
```

### ZIP with password (AES-256)

```typescript
// Pack with encryption
await pack('/path/to/source', '/path/to/encrypted.zip', 'zip', {
  password: 'my-secret',
});

// Unpack encrypted archive
await unpack('/path/to/encrypted.zip', '/path/to/output', true, {
  password: 'my-secret',
});
```

### List archive contents

```typescript
const entries = await listContents('/path/to/archive.zip');

for (const entry of entries) {
  console.log(entry.path, entry.size, entry.isDirectory, entry.isEncrypted);
}
```

### Detect format

```typescript
const format = await detectFormat('/path/to/archive');
// Returns: 'tar' | 'gz' | 'bz2' | 'tar.gz' | 'tar.bz2' | 'zip' | 'sevenz'
```

### Test archive integrity

```typescript
const isValid = await testArchive('/path/to/archive.zip');
// For encrypted archives:
const isValid = await testArchive('/path/to/encrypted.zip', 'password');
```

### Progress callback

```typescript
await pack('/path/to/source', '/path/to/output.tar.bz2', 'tar.bz2', undefined,
  (bytesProcessed, totalBytes) => {
    const progress = totalBytes > 0 ? bytesProcessed / totalBytes : 0;
    console.log(`${Math.round(progress * 100)}%`);
  }
);

await unpack('/path/to/archive.tar.bz2', '/path/to/output', true, undefined,
  (bytesProcessed, totalBytes) => {
    console.log(`${bytesProcessed} / ${totalBytes}`);
  }
);
```

## API Reference

### `pack(src, dest, format, options?, onProgress?)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `src` | `string` | Source file or directory path |
| `dest` | `string` | Output archive path |
| `format` | `ArchiveFormat` | `'tar'` \| `'tar.bz2'` \| `'tar.gz'` \| `'bz2'` \| `'gz'` \| `'zip'` \| `'sevenz'` |
| `options` | `ArchiveOptions?` | `{ password?, compressionLevel? }` |
| `onProgress` | `ProgressCallback?` | `(bytesProcessed, totalBytes) => void` |

Returns `Promise<ArchiveResult>`.

### `unpack(src, dest, overwrite, options?, onProgress?)`

| Parameter | Type | Description |
|-----------|------|-------------|
| `src` | `string` | Source archive path |
| `dest` | `string` | Output directory path |
| `overwrite` | `boolean` | Overwrite existing files |
| `options` | `ArchiveOptions?` | `{ password? }` |
| `onProgress` | `ProgressCallback?` | `(bytesProcessed, totalBytes) => void` |

Returns `Promise<ArchiveResult>`.

### `listContents(src, options?)`

Returns `Promise<ArchiveEntry[]>` where each entry has:
- `path: string`
- `size: number`
- `compressedSize: number`
- `isDirectory: boolean`
- `isEncrypted: boolean`

### `detectFormat(src)`

Returns `Promise<ArchiveFormat>`.

### `testArchive(src, password?)`

Returns `Promise<boolean>`.

## Types

```typescript
type ArchiveFormat = 'tar' | 'gz' | 'bz2' | 'tar.gz' | 'tar.bz2' | 'zip' | 'sevenz';

interface ArchiveOptions {
  password?: string;
  compressionLevel?: number;
}

interface ArchiveEntry {
  path: string;
  size: number;
  compressedSize: number;
  isDirectory: boolean;
  isEncrypted: boolean;
}

interface ArchiveResult {
  success: boolean;
  outputPath: string;
  errorMessage: string;
  bytesProcessed: number;
}

type ProgressCallback = (bytesProcessed: number, totalBytes: number) => void;
```

## Vendored Libraries

All compression libraries are vendored (no external dependencies):

- **bzip2** — Julian Seward's bzip2 1.0.8
- **minizip-ng** — zlib-ng/minizip-ng (ZIP read/write with AES-256)
- **LZMA SDK** — Igor Pavlov's LZMA SDK (7z decompression)
- **zlib** — system zlib (via iOS/Android SDK)

## Requirements

- React Native >= 0.76
- react-native-nitro-modules >= 0.20.0
- iOS >= 13.0
- Android minSdk 21

## License

MIT
