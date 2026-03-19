import type { HybridObject } from 'react-native-nitro-modules'

export type ArchiveFormat = 'tar' | 'gz' | 'bz2' | 'tar.gz' | 'tar.bz2' | 'zip' | 'sevenz'

export interface ArchiveOptions {
  password?: string
  compressionLevel?: number
}

export interface ArchiveEntry {
  path: string
  size: number
  isDirectory: boolean
  compressedSize: number
  isEncrypted: boolean
}

export interface ArchiveResult {
  success: boolean
  outputPath: string
  errorMessage: string
  bytesProcessed: number
}

export type ProgressCallback = (
  bytesProcessed: number,
  totalBytes: number
) => void

export interface Archive
  extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  unpack(
    sourcePath: string,
    targetPath: string,
    overwrite: boolean,
    options?: ArchiveOptions,
    onProgress?: ProgressCallback
  ): Promise<ArchiveResult>
  pack(
    sourcePath: string,
    targetPath: string,
    format: ArchiveFormat,
    options?: ArchiveOptions,
    onProgress?: ProgressCallback
  ): Promise<ArchiveResult>
  listContents(sourcePath: string, options?: ArchiveOptions): Promise<ArchiveEntry[]>
  detectFormat(sourcePath: string): Promise<ArchiveFormat>
  testArchive(sourcePath: string, password?: string): Promise<boolean>
}
