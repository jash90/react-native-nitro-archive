import type { HybridObject } from 'react-native-nitro-modules'

export interface TarBz2Result {
  success: boolean
  path: string
  error: string
}

export interface TarBz2
  extends HybridObject<{ ios: 'c++'; android: 'kotlin' }> {
  unpack(
    sourcePath: string,
    targetPath: string,
    overwrite: boolean
  ): Promise<TarBz2Result>
  pack(sourcePath: string, targetPath: string): Promise<TarBz2Result>
  listContents(sourcePath: string): Promise<string[]>

  unpackWithProgress(
    sourcePath: string,
    targetPath: string,
    overwrite: boolean,
    onProgress: (progress: number) => void
  ): Promise<TarBz2Result>
  packWithProgress(
    sourcePath: string,
    targetPath: string,
    onProgress: (progress: number) => void
  ): Promise<TarBz2Result>
}
