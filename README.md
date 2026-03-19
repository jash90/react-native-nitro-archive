# react-native-tar-bz2

A React Native [Nitro Module](https://nitro.margelo.com/) for tar.bz2 archive operations — pack, unpack, and list contents — with optional progress callbacks.

- **iOS**: C++ implementation using system `libbz2`
- **Android**: Kotlin implementation using [Apache Commons Compress](https://commons.apache.org/proper/commons-compress/)

## Installation

```sh
npm install react-native-tar-bz2 react-native-nitro-modules
```

### Expo

The package ships with an Expo config plugin that auto-configures Android native build settings:

```json
// app.json
{
  "plugins": ["react-native-tar-bz2"]
}
```

Then run `npx expo prebuild`.

### Bare React Native

#### iOS

```sh
cd ios && pod install
```

#### Android

Add to your app-level `build.gradle`:

```groovy
dependencies {
    implementation 'org.apache.commons:commons-compress:1.26.0'
}
```

## Usage

```typescript
import { unpack, pack, listContents } from 'react-native-tar-bz2';

// Unpack an archive
const result = await unpack('/path/to/archive.tar.bz2', '/path/to/output', true);
// result: { success: boolean, path: string, error: string }

// Pack a directory into an archive
const result = await pack('/path/to/directory', '/path/to/output.tar.bz2');

// List archive contents
const entries = await listContents('/path/to/archive.tar.bz2');
// entries: string[]
```

### With progress callbacks

```typescript
import { unpackWithProgress, packWithProgress } from 'react-native-tar-bz2';

await unpackWithProgress(
  '/path/to/archive.tar.bz2',
  '/path/to/output',
  true, // overwrite
  (progress) => console.log(`Unpacking: ${Math.round(progress * 100)}%`)
);

await packWithProgress(
  '/path/to/directory',
  '/path/to/output.tar.bz2',
  (progress) => console.log(`Packing: ${Math.round(progress * 100)}%`)
);
```

## API

### `unpack(src, dest, overwrite?)`

Extracts a `.tar.bz2` archive to a destination directory.

| Param | Type | Default | Description |
|-------|------|---------|-------------|
| `src` | `string` | — | Path to the `.tar.bz2` file |
| `dest` | `string` | — | Destination directory |
| `overwrite` | `boolean` | `true` | Overwrite existing files |

Returns `Promise<TarBz2Result>`

### `pack(src, dest)`

Creates a `.tar.bz2` archive from a file or directory.

| Param | Type | Description |
|-------|------|-------------|
| `src` | `string` | Source file or directory |
| `dest` | `string` | Output `.tar.bz2` file path |

Returns `Promise<TarBz2Result>`

### `listContents(src)`

Lists all entries in a `.tar.bz2` archive.

| Param | Type | Description |
|-------|------|-------------|
| `src` | `string` | Path to the `.tar.bz2` file |

Returns `Promise<string[]>`

### `unpackWithProgress(src, dest, overwrite, onProgress)`

Same as `unpack` but with a progress callback (`0.0` to `1.0`).

### `packWithProgress(src, dest, onProgress)`

Same as `pack` but with a progress callback (`0.0` to `1.0`).

### `TarBz2Result`

```typescript
interface TarBz2Result {
  success: boolean;
  path: string;    // output path on success
  error: string;   // error message on failure
}
```

## Requirements

- React Native >= 0.76
- `react-native-nitro-modules` >= 0.20.0
- iOS 13.0+
- Android: minSdk as per your project

## License

MIT
