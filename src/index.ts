import { NitroModules } from 'react-native-nitro-modules'
import type { Archive, ArchiveFormat, ArchiveOptions, ArchiveEntry, ArchiveResult, ProgressCallback } from './specs/Archive.nitro'

const ArchiveModule = NitroModules.createHybridObject<Archive>('Archive')

export const unpack = (
  src: string,
  dest: string,
  overwrite = true,
  options?: ArchiveOptions,
  onProgress?: ProgressCallback
) => ArchiveModule.unpack(src, dest, overwrite, options, onProgress)

export const pack = (
  src: string,
  dest: string,
  format: ArchiveFormat = 'tar.bz2',
  options?: ArchiveOptions,
  onProgress?: ProgressCallback
) => ArchiveModule.pack(src, dest, format, options, onProgress)

export const listContents = (src: string, options?: ArchiveOptions) =>
  ArchiveModule.listContents(src, options)

export const detectFormat = (src: string) => ArchiveModule.detectFormat(src)

export const testArchive = (src: string, password?: string) =>
  ArchiveModule.testArchive(src, password)

export type { Archive, ArchiveFormat, ArchiveOptions, ArchiveEntry, ArchiveResult, ProgressCallback }
