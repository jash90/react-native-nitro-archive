import { NitroModules } from 'react-native-nitro-modules'
import type { TarBz2 } from './specs/TarBz2.nitro'

const TarBz2Module = NitroModules.createHybridObject<TarBz2>('TarBz2')

export const unpack = (src: string, dest: string, overwrite = true) =>
  TarBz2Module.unpack(src, dest, overwrite)

export const pack = (src: string, dest: string) =>
  TarBz2Module.pack(src, dest)

export const listContents = (src: string) =>
  TarBz2Module.listContents(src)

export const unpackWithProgress = (
  src: string,
  dest: string,
  overwrite: boolean,
  onProgress: (progress: number) => void
) => TarBz2Module.unpackWithProgress(src, dest, overwrite, onProgress)

export const packWithProgress = (
  src: string,
  dest: string,
  onProgress: (progress: number) => void
) => TarBz2Module.packWithProgress(src, dest, onProgress)

export type { TarBz2, TarBz2Result } from './specs/TarBz2.nitro'
